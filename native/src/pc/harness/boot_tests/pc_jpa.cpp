// Particles on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.7): the COS_SMOKE=jpa-sweep test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every JPC of the
// disc (JPAC1-00: /res/Particle/common.jpc and every Pscene*.jpc, found with DVDReadDir) is loaded
// with JKRDvdToMainRam and read twice:
// - independently: plain big-endian loads at the format's offsets (the archive header, each
//   JEFFjpa1 emitter with its blocks, each TEX1 texture), no game struct;
// - through the game, as dPa_control_c does: a JPAResourceManager made from the bytes (common.jpc
//   in the sweep heap, each scene in a solid heap adjusted afterwards, as createRoomScene), a
//   JPAEmitterManager with the game's pool sizes (3000 particles, 150 emitters, 200 fields) whose
//   resource manager 1 is the scene's.
// For every emitter resource: the user index, key/field/texture counts and the block order; every
// getter of every block (BEM1 dynamics, FLD1 fields, KFA1 keys with all their key data, BSP1 base
// shape with its texture-animation indices and both colour tables at each key frame, ESP1 extra
// shape with its derived rates, SSP1 sweep shape, ETX1 indirect-texture shape, TDB1 texture
// indices) against the independent reading; every texture's name and ResTIMG through JPATexture
// and its JUTTexture. Then every emitter is created with createSimpleEmitterID (its data flags and
// rate after JPABaseEmitter::create checked against BEM1) and calculated alone for 30 frames with
// JPAEmitterManager::calc; every live particle and child must have a finite position, velocity
// and age. Emitters are then force-deleted and the scene's resource manager cleared, as
// removeRoomScene does.
// <COS_RUN_DIR>/jpa_sweep.txt gets what the game read (JPC/EMTR/TEX lines in disc_manifest.py's
// names); native/tools/run.sh compares it with the manifest (disc_manifest.py --check-jpa).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JParticle/JPABaseShape.h"
#include "JSystem/JParticle/JPADynamicsBlock.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAEmitterLoader.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAExTexShape.h"
#include "JSystem/JParticle/JPAExtraShape.h"
#include "JSystem/JParticle/JPAFieldBlock.h"
#include "JSystem/JParticle/JPAKeyBlock.h"
#include "JSystem/JParticle/JPAParticle.h"
#include "JSystem/JParticle/JPAResourceManager.h"
#include "JSystem/JParticle/JPASweepShape.h"
#include "JSystem/JParticle/JPATexture.h"
#include "m_Do/m_Do_ext.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h), not in the
// game's operator new (the JKR heaps the sweep loads into).
using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 32 * 1024 * 1024;
constexpr int kCalcFrames = 30;

int sErrors = 0;
const char* sWhere = "";

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] jpa-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

int16_t rdS16(const uint8_t* p) {
    return (int16_t)rd16(p);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

uint32_t bits(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

float rdF(const uint8_t* p) {
    uint32_t u = rd32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

constexpr uint32_t tag(char a, char b, char c, char d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

String tagText(uint32_t t) {
    char s[5] = {(char)(t >> 24), (char)(t >> 16), (char)(t >> 8), (char)t, 0};
    return s;
}

// Context of the check being made, for the messages.
struct Ctx {
    int emitter;
    const char* block;
};
Ctx sCtx = {-1, ""};

void eqU(const char* what, uint32_t got, uint32_t want) {
    if (got != want) {
        fail("emitter %d %s %s: game 0x%x, file 0x%x", sCtx.emitter, sCtx.block, what, got, want);
    }
}

// Floats compare by their bits: the file's value must arrive unchanged.
void eqF(const char* what, float got, float want) {
    if (bits(got) != bits(want)) {
        fail("emitter %d %s %s: game %g (0x%08x), file %g (0x%08x)", sCtx.emitter, sCtx.block, what,
             got, bits(got), want, bits(want));
    }
}

void eqF(const char* what, float got, const uint8_t* p) {
    eqF(what, got, rdF(p));
}

void eqVec(const char* what, const JGeometry::TVec3<f32>& got, const uint8_t* p) {
    eqF(what, got.x, p);
    eqF(what, got.y, p + 4);
    eqF(what, got.z, p + 8);
}

void eqColor(const char* what, GXColor got, const uint8_t* p) {
    uint32_t g = ((uint32_t)got.r << 24) | ((uint32_t)got.g << 16) | ((uint32_t)got.b << 8) | got.a;
    eqU(what, g, rd32(p));
}

// ---- the independent reading -------------------------------------------------------------------

struct RawBlock {
    uint32_t tag;
    const uint8_t* base; // the block header
    uint32_t size;
    const uint8_t* data() const { return base + 0x0C; }
};

struct RawEmitter {
    const uint8_t* base;
    uint16_t resId;
    uint32_t numBlocks;
    uint8_t keys, fields, textures;
    Vector<RawBlock> blocks;
};

struct RawTexture {
    const uint8_t* base;
    uint32_t size;
    String name;
};

struct RawJpc {
    Vector<RawEmitter> emitters;
    Vector<RawTexture> textures;
};

bool parseRaw(const uint8_t* b, uint32_t length, RawJpc& raw) {
    if (length < 0x20 || memcmp(b, "JPAC1-00", 8) != 0) {
        fail("not a JPAC1-00 file");
        return false;
    }
    uint16_t numEmitters = rd16(b + 8);
    uint16_t numTextures = rd16(b + 0xA);
    uint32_t o = 0x20;
    for (uint16_t i = 0; i < numEmitters; i++) {
        if (o + 0x20 > length || memcmp(b + o, "JEFFjpa1", 8) != 0) {
            fail("emitter %u at 0x%x: no JEFFjpa1", i, o);
            return false;
        }
        RawEmitter e;
        e.base = b + o;
        e.numBlocks = rd32(b + o + 0xC);
        e.keys = b[o + 0x14];
        e.fields = b[o + 0x15];
        e.textures = b[o + 0x16];
        e.resId = rd16(b + o + 0x18);
        uint32_t bo = o + 0x20;
        for (uint32_t k = 0; k < e.numBlocks; k++) {
            if (bo + 0xC > length || rd32(b + bo + 4) < 0xC || bo + rd32(b + bo + 4) > length) {
                fail("emitter %u block %u at 0x%x lies past the end", i, k, bo);
                return false;
            }
            e.blocks.push_back({rd32(b + bo), b + bo, rd32(b + bo + 4)});
            bo += rd32(b + bo + 4);
        }
        raw.emitters.push_back(e);
        o = bo;
    }
    for (uint16_t i = 0; i < numTextures; i++) {
        if (o + 0x40 > length || memcmp(b + o, "TEX1", 4) != 0 || rd32(b + o + 4) < 0x40 ||
            o + rd32(b + o + 4) > length) {
            fail("texture %u at 0x%x: no TEX1 block", i, o);
            return false;
        }
        RawTexture t;
        t.base = b + o;
        t.size = rd32(b + o + 4);
        t.name.assign((const char*)b + o + 0xC, strnlen((const char*)b + o + 0xC, 0x14));
        raw.textures.push_back(t);
        o += t.size;
    }
    return true;
}

// ---- the blocks through the game ---------------------------------------------------------------

void checkDynamics(JPADynamicsBlock* dyn, const uint8_t* d) {
    sCtx.block = "BEM1";
    uint32_t flags = rd32(d);
    eqU("getDataFlag", dyn->getDataFlag(), flags);
    eqU("getVolumeType", dyn->getVolumeType(), (flags >> 8) & 7);
    eqF("getVolumeSweep", dyn->getVolumeSweep(), d + 0x04);
    eqF("getVolumeMinRad", dyn->getVolumeMinRad(), d + 0x08);
    eqU("getVolumeSize", dyn->getVolumeSize(), rd16(d + 0x0C));
    eqU("getDivNumber", dyn->getDivNumber(), rd16(d + 0x0E));
    eqF("getRate", dyn->getRate(), d + 0x10);
    eqF("getRateRndm", dyn->getRateRndm(), d + 0x14);
    eqU("getRateStep", dyn->getRateStep(), d[0x18]);
    eqU("getMaxFrame", (uint32_t)(int32_t)dyn->getMaxFrame(), (uint32_t)(int32_t)rdS16(d + 0x1A));
    eqU("getStartFrame", dyn->getStartFrame(), (uint32_t)(int32_t)rdS16(d + 0x1C));
    eqU("getLifeTime", dyn->getLifeTime(), (uint32_t)(int32_t)rdS16(d + 0x1E));
    eqF("getLifeTimeRndm", dyn->getLifeTimeRndm(), d + 0x20);
    eqF("getInitVelOmni", dyn->getInitVelOmni(), d + 0x24);
    eqF("getInitVelAxis", dyn->getInitVelAxis(), d + 0x28);
    eqF("getInitVelRndm", dyn->getInitVelRndm(), d + 0x2C);
    eqF("getInitVelDir", dyn->getInitVelDir(), d + 0x30);
    eqF("getInitVelRatio", dyn->getInitVelRatio(), d + 0x34);
    eqF("getSpread", dyn->getSpread(), d + 0x38);
    eqF("getAirResist", dyn->getAirResist(), d + 0x3C);
    eqF("getAirResistRndm", dyn->getAirResistRndm(), d + 0x40);
    eqF("getMoment", dyn->getMoment(), d + 0x44);
    eqF("getMomentRndm", dyn->getMomentRndm(), d + 0x48);
    eqF("getAccel", dyn->getAccel(), d + 0x4C);
    eqF("getAccelRndm", dyn->getAccelRndm(), d + 0x50);
    JGeometry::TVec3<f32> v;
    dyn->getEmitterScl(v);
    eqVec("getEmitterScl", v, d + 0x54);
    dyn->getEmitterTrs(v);
    eqVec("getEmitterTrs", v, d + 0x60);
    dyn->getEmitterDir(v);
    eqVec("getEmitterDir", v, d + 0x6C);
    JGeometry::TVec3<s16> r;
    dyn->getEmitterRot(r);
    eqU("getEmitterRot.x", (uint16_t)r.x, rd16(d + 0x78));
    eqU("getEmitterRot.y", (uint16_t)r.y, rd16(d + 0x7A));
    eqU("getEmitterRot.z", (uint16_t)r.z, rd16(d + 0x7C));
}

void checkField(JPAFieldBlock* fld, const uint8_t* d) {
    sCtx.block = "FLD1";
    uint32_t flags = rd32(d);
    eqU("getType", fld->getType(), flags & 0x0F);
    eqU("getVelType", fld->getVelType(), (flags >> 8) & 0x03);
    eqU("getSttFlag", fld->getSttFlag(), (flags >> 16) & 0xFFFF);
    eqF("getMag", fld->getMag(), d + 0x04);
    eqF("getMagRndm", fld->getMagRndm(), d + 0x08);
    eqF("getMaxDist", fld->getMaxDist(), d + 0x0C);
    JGeometry::TVec3<f32> v;
    fld->getPos(v);
    eqVec("getPos", v, d + 0x10);
    fld->getDir(v);
    eqVec("getDir", v, d + 0x1C);
    eqF("getVal1", fld->getVal1(), d + 0x28);
    eqF("getVal2", fld->getVal2(), d + 0x2C);
    eqF("getVal3", fld->getVal3(), d + 0x30);
    eqF("getFadeIn", fld->getFadeIn(), d + 0x34);
    eqF("getFadeOut", fld->getFadeOut(), d + 0x38);
    eqF("getEnTime", fld->getEnTime(), d + 0x3C);
    eqF("getDisTime", fld->getDisTime(), d + 0x40);
    eqU("getCycle", fld->getCycle(), d[0x44]);
}

void checkKey(JPAKeyBlock* key, const RawBlock& b) {
    sCtx.block = "KFA1";
    const uint8_t* d = b.data();
    eqU("getID", key->getID(), d[0]);
    eqU("getNumber", key->getNumber(), d[4]);
    eqU("isLoopEnable", key->isLoopEnable(), d[6] != 0);
    uint32_t n = (uint32_t)d[4] * 4;
    if (0x20 + n * 4 > b.size) {
        fail("emitter %d KFA1: %u keys do not fit in %u bytes", sCtx.emitter, d[4], b.size);
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        eqF("key data", key->getKeyDataPtr()[i], b.base + 0x20 + i * 4);
    }
}

void checkColorTable(const char* what, JPABaseShape* bsp, bool prm, const RawBlock& b,
                     int16_t offset, uint8_t numKeys, int16_t maxFrame) {
    // At a key's frame the table holds the key's colour (makeColorTable).
    for (uint8_t k = 0; k < numKeys; k++) {
        const uint8_t* key = b.base + offset + k * 6;
        if (key + 6 > b.base + b.size) {
            fail("emitter %d BSP1 %s key %u lies past the block", sCtx.emitter, what, k);
            return;
        }
        int16_t frame = rdS16(key);
        if (frame < 0 || frame > maxFrame) {
            continue;
        }
        eqColor(what, prm ? bsp->getPrmColor(frame) : bsp->getEnvColor(frame), key + 2);
    }
}

void checkBaseShape(JPABaseShape* bsp, const RawBlock& b) {
    sCtx.block = "BSP1";
    const uint8_t* d = b.data();
    uint32_t flags = rd32(d);
    uint16_t blend = rd16(d + 0x12);
    uint8_t alpha = d[0x14], z = d[0x17], tex = d[0x18], color = d[0x1B];
    eqU("getType", bsp->getType(), flags & 0x0F);
    eqU("getDirType", bsp->getDirType(), (flags >> 4) & 7);
    eqU("getRotType", bsp->getRotType(), (flags >> 7) & 7);
    eqU("getBasePlaneType", bsp->getBasePlaneType(), (flags >> 10) & 1);
    eqF("getBaseSizeX", bsp->getBaseSizeX(), d + 0x08);
    eqF("getBaseSizeY", bsp->getBaseSizeY(), d + 0x0C);
    eqU("getLoopOffset", (uint16_t)bsp->getLoopOffset(), rd16(d + 0x10));
    eqU("getColLoopOffset", (uint16_t)bsp->getColLoopOffset(), (flags & 0x0800) ? 0xFFFF : 0);
    eqU("getTexLoopOffset", (uint16_t)bsp->getTexLoopOffset(), (flags & 0x2000) ? 0xFFFF : 0);
    eqU("getListOrder", bsp->getListOrder(), flags & 0x200000);
    eqU("getChildOrder", bsp->getChildOrder(), flags & 0x400000);
    eqU("isEnableAnmTone", bsp->isEnableAnmTone(), flags & 0x80000);
    eqU("isEnableProjection", bsp->isEnableProjection(), flags & 0x100000);
    eqU("isClipOn", bsp->isClipOn(), flags & 0x800000);
    eqU("isEnableTexScrollAnm", bsp->isEnableTexScrollAnm(), flags & 0x1000000);
    eqU("getBlendMode1", bsp->getBlendMode1(), JPABaseShape::stBlendMode[blend & 3]);
    eqU("getSrcBlendFactor1", bsp->getSrcBlendFactor1(),
        ((blend >> 2) & 0xF) < 10 ? JPABaseShape::stBlendFactor[(blend >> 2) & 0xF] : 0xFFFFFFFFu);
    eqU("getDstBlendFactor1", bsp->getDstBlendFactor1(),
        ((blend >> 6) & 0xF) < 10 ? JPABaseShape::stBlendFactor[(blend >> 6) & 0xF] : 0xFFFFFFFFu);
    eqU("getBlendOp1", bsp->getBlendOp1(), JPABaseShape::stLogicOp[(blend >> 10) & 0xF]);
    eqU("isEnableAlphaUpdate", bsp->isEnableAlphaUpdate(), (blend >> 14) & 1);
    eqU("isEnableZCmp", bsp->isEnableZCmp(), z & 1);
    eqU("getZCmpFunction", bsp->getZCmpFunction(), JPABaseShape::stCompare[(z >> 1) & 7]);
    eqU("isEnableZCmpUpdate", bsp->isEnableZCmpUpdate(), (z >> 4) & 1);
    eqU("getZCompLoc", bsp->getZCompLoc(), (z >> 5) & 1);
    eqU("getAlphaCmpComp0", bsp->getAlphaCmpComp0(), JPABaseShape::stCompare[alpha & 7]);
    eqU("getAlphaCmpComp1", bsp->getAlphaCmpComp1(), JPABaseShape::stCompare[(alpha >> 5) & 7]);
    eqU("getAlphaCmpOp", bsp->getAlphaCmpOp(), JPABaseShape::stAlphaOp[(alpha >> 3) & 3]);
    eqU("getAlphaCmpRef0", bsp->getAlphaCmpRef0(), d[0x15]);
    eqU("getAlphaCmpRef1", bsp->getAlphaCmpRef1(), d[0x16]);
    eqU("isEnableTextureAnm", bsp->isEnableTextureAnm(), tex & 1);
    eqU("textureIsEmpty", bsp->textureIsEmpty(), !(tex & 2));
    eqU("getTextureAnmType", bsp->getTextureAnmType(), (tex >> 2) & 7);
    eqU("getTextureAnmKeyNum", bsp->getTextureAnmKeyNum(), d[0x19]);
    eqU("getTextureIndex", bsp->getTextureIndex(), d[0x1A]);
    eqU("isEnablePrm", bsp->isEnablePrm(), color & 1);
    eqU("isEnablePrmAnm", bsp->isEnablePrmAnm(), color & 2);
    eqU("isEnableEnv", bsp->isEnableEnv(), color & 4);
    eqU("isEnableEnvAnm", bsp->isEnableEnvAnm(), color & 8);
    eqU("getColorRegAnmType", bsp->getColorRegAnmType(), (color >> 4) & 7);
    eqU("getColorRegAnmMaxFrm", (uint16_t)bsp->getColorRegAnmMaxFrm(), rd16(d + 0x1E));
    eqColor("getPrmColor", bsp->getPrmColor(), d + 0x20);
    eqColor("getEnvColor", bsp->getEnvColor(), d + 0x24);
    eqF("getTilingX", bsp->getTilingX(), d + 0x28);
    eqF("getTilingY", bsp->getTilingY(), d + 0x2C);
    eqF("getTexStaticTransX", bsp->getTexStaticTransX(), d + 0x30);
    eqF("getTexStaticTransY", bsp->getTexStaticTransY(), d + 0x34);
    eqF("getTexStaticScaleX", bsp->getTexStaticScaleX(), d + 0x38);
    eqF("getTexStaticScaleY", bsp->getTexStaticScaleY(), d + 0x3C);
    eqF("getTexScrollTransX", bsp->getTexScrollTransX(), d + 0x40);
    eqF("getTexScrollTransY", bsp->getTexScrollTransY(), d + 0x44);
    eqF("getTexScrollScaleX", bsp->getTexScrollScaleX(), d + 0x48);
    eqF("getTexScrollScaleY", bsp->getTexScrollScaleY(), d + 0x4C);
    eqF("getTexScrollRotate", bsp->getTexScrollRotate(), d + 0x50);
    if ((tex & 1) && d[0x19] != 0) {
        for (uint8_t i = 0; i < d[0x19]; i++) {
            eqU("getTextureIndex(i)", bsp->getTextureIndex(i), b.base[0x60 + i]);
        }
    }
    int16_t maxFrame = rdS16(d + 0x1E);
    if (color & 2) {
        checkColorTable("getPrmColor(frame)", bsp, true, b, rdS16(d + 0x04), d[0x1C], maxFrame);
    }
    if (color & 8) {
        checkColorTable("getEnvColor(frame)", bsp, false, b, rdS16(d + 0x06), d[0x1D], maxFrame);
    }
}

void checkExtraShape(JPAExtraShape* esp, const uint8_t* d) {
    sCtx.block = "ESP1";
    uint32_t flags = rd32(d);
    eqU("isEnableScale", esp->isEnableScale(), flags & 0x100);
    eqU("isDiffXY", esp->isDiffXY(), flags & 0x200);
    eqU("isEnableScaleAnmX", esp->isEnableScaleAnmX(), flags & 0x800);
    eqU("isEnableScaleAnmY", esp->isEnableScaleAnmY(), flags & 0x400);
    eqU("isEnableScaleBySpeedX", esp->isEnableScaleBySpeedX(), flags & 0x2000);
    eqU("isEnableScaleBySpeedY", esp->isEnableScaleBySpeedY(), flags & 0x1000);
    eqU("getAnmTypeX", esp->getAnmTypeX(), (flags >> 18) & 1);
    eqU("getAnmTypeY", esp->getAnmTypeY(), (flags >> 19) & 1);
    eqU("getPivotX", esp->getPivotX(), (flags >> 14) & 3);
    eqU("getPivotY", esp->getPivotY(), (flags >> 16) & 3);
    eqU("isEnableAlpha", esp->isEnableAlpha(), flags & 1);
    eqU("isEnableSinWave", esp->isEnableSinWave(), flags & 2);
    eqU("getAlphaWaveType", esp->getAlphaWaveType(), (flags >> 2) & 3);
    eqU("isEnableRotate", esp->isEnableRotate(), flags & 0x01000000);
    eqF("getAlphaInTiming", esp->getAlphaInTiming(), d + 0x08);
    eqF("getAlphaOutTiming", esp->getAlphaOutTiming(), d + 0x0C);
    eqF("getAlphaInValue", esp->getAlphaInValue(), d + 0x10);
    eqF("getAlphaBaseValue", esp->getAlphaBaseValue(), d + 0x14);
    eqF("getAlphaWaveParam1", esp->getAlphaWaveParam1(), d + 0x1C);
    eqF("getAlphaWaveParam2", esp->getAlphaWaveParam2(), d + 0x20);
    eqF("getAlphaWaveParam3", esp->getAlphaWaveParam3(), d + 0x24);
    eqF("getAlphaWaveRandom", esp->getAlphaWaveRandom(), d + 0x28);
    eqF("getScaleInTiming", esp->getScaleInTiming(), d + 0x2C);
    eqF("getScaleOutTiming", esp->getScaleOutTiming(), d + 0x30);
    eqF("getScaleInValueX", esp->getScaleInValueX(), d + 0x34);
    eqF("getScaleInValueY", esp->getScaleInValueY(), d + 0x3C);
    eqF("getRandomScale", esp->getRandomScale(), d + 0x44);
    eqU("getAnmCycleX", (uint16_t)esp->getAnmCycleX(), rd16(d + 0x48));
    eqU("getAnmCycleY", (uint16_t)esp->getAnmCycleY(), rd16(d + 0x4A));
    eqF("getRotateAngle", esp->getRotateAngle(), d + 0x4C);
    eqF("getRotateSpeed", esp->getRotateSpeed(), d + 0x50);
    eqF("getRotateRandomAngle", esp->getRotateRandomAngle(), d + 0x54);
    eqF("getRotateRandomSpeed", esp->getRotateRandomSpeed(), d + 0x58);
    eqF("getRotateDirection", esp->getRotateDirection(), d + 0x5C);
    // The rates JPAExtraShapeArc's constructor derives, with the file's values.
    float inT = rdF(d + 0x08), outT = rdF(d + 0x0C), inV = rdF(d + 0x10), baseV = rdF(d + 0x14);
    float outV = rdF(d + 0x18), sInT = rdF(d + 0x2C), sOutT = rdF(d + 0x30);
    float sInX = rdF(d + 0x34), sOutX = rdF(d + 0x38), sInY = rdF(d + 0x3C), sOutY = rdF(d + 0x40);
    eqF("getAlphaIncreaseRate", esp->getAlphaIncreaseRate(), inT != 0.0f ? (baseV - inV) / inT : 1.0f);
    eqF("getAlphaDecreaseRate", esp->getAlphaDecreaseRate(),
        outT != 1.0f ? (outV - baseV) / (1.0f - outT) : 1.0f);
    eqF("getIncreaseRateX", esp->getIncreaseRateX(), sInT != 0.0f ? (1.0f - sInX) / sInT : 1.0f);
    eqF("getIncreaseRateY", esp->getIncreaseRateY(), sInT != 0.0f ? (1.0f - sInY) / sInT : 1.0f);
    eqF("getDecreaseRateX", esp->getDecreaseRateX(),
        sOutT != 1.0f ? (sOutX - 1.0f) / (1.0f - sOutT) : 1.0f);
    eqF("getDecreaseRateY", esp->getDecreaseRateY(),
        sOutT != 1.0f ? (sOutY - 1.0f) / (1.0f - sOutT) : 1.0f);
}

void checkSweepShape(JPASweepShape* ssp, const uint8_t* d) {
    sCtx.block = "SSP1";
    uint32_t flags = rd32(d);
    eqU("getType", ssp->getType(), flags & 0x0F);
    eqU("getDirType", ssp->getDirType(), (flags >> 4) & 7);
    eqU("getRotType", ssp->getRotType(), (flags >> 7) & 7);
    eqU("getBasePlaneType", ssp->getBasePlaneType(), (flags >> 10) & 1);
    eqU("isEnableField", ssp->isEnableField(), flags & 0x00200000);
    eqU("isEnableDrawParent", ssp->isEnableDrawParent(), flags & 0x00080000);
    eqU("isClipOn", ssp->isClipOn(), flags & 0x00100000);
    eqU("isEnableScaleOut", ssp->isEnableScaleOut(), flags & 0x00400000);
    eqU("isEnableAlphaOut", ssp->isEnableAlphaOut(), flags & 0x00800000);
    eqU("isEnableRotate", ssp->isEnableRotate(), flags & 0x01000000);
    eqU("isInheritedScale", ssp->isInheritedScale(), flags & 0x00010000);
    eqU("isInheritedAlpha", ssp->isInheritedAlpha(), flags & 0x00020000);
    eqU("isInheritedRGB", ssp->isInheritedRGB(), flags & 0x00040000);
    eqF("getPosRndm", ssp->getPosRndm(), d + 0x04);
    eqF("getBaseVel", ssp->getBaseVel(), d + 0x08);
    eqF("getBaseVelRndm", ssp->getBaseVelRndm(), d + 0x0C);
    eqF("getVelInfRate", ssp->getVelInfRate(), d + 0x10);
    eqF("getGravity", ssp->getGravity(), d + 0x14);
    eqF("getTiming", ssp->getTiming(), d + 0x18);
    eqU("getLife", (uint16_t)ssp->getLife(), rd16(d + 0x1C));
    eqU("getRate", (uint16_t)ssp->getRate(), rd16(d + 0x1E));
    eqU("getStep", ssp->getStep(), d[0x20]);
    eqF("getScaleX", ssp->getScaleX(), d + 0x24);
    eqF("getScaleY", ssp->getScaleY(), d + 0x28);
    eqF("getRotateSpeed", ssp->getRotateSpeed(), d + 0x2C);
    eqF("getInheritScale", ssp->getInheritScale(), d + 0x30);
    eqF("getInheritAlpha", ssp->getInheritAlpha(), d + 0x34);
    eqF("getInheritRGB", ssp->getInheritRGB(), d + 0x38);
    eqColor("getPrm", ssp->getPrm(), d + 0x3C);
    eqColor("getEnv", ssp->getEnv(), d + 0x40);
    eqU("getPrmAlpha", ssp->getPrmAlpha(), d[0x3F]);
    eqU("getEnvAlpha", ssp->getEnvAlpha(), d[0x43]);
    eqU("getTextureIndex", ssp->getTextureIndex(), d[0x44]);
}

void checkExTexShape(JPAExTexShape* etx, const uint8_t* d) {
    sCtx.block = "ETX1";
    uint32_t flags = rd32(d);
    eqU("getIndTexMode", etx->getIndTexMode(), flags & 3);
    eqU("getIndTexMtxID", etx->getIndTexMtxID(), JPAExTexShapeArc::indMtxID[(flags >> 2) & 3]);
    eqU("isEnableSecondTex", etx->isEnableSecondTex(), flags & 0x100);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            eqF("getIndTexMtx", (*etx->getIndTexMtx())[i][j], d + 0x04 + (i * 3 + j) * 4);
        }
    }
    eqU("getExpScale", (uint8_t)etx->getExpScale(), d[0x1C]);
    eqU("getIndTextureID", etx->getIndTextureID(), d[0x20]);
    eqU("getSubTextureID", etx->getSubTextureID(), d[0x21]);
    eqU("getSecondTexIndex", etx->getSecondTexIndex(), d[0x22]);
}

// Every block of one emitter resource; fills the tag list the report gets.
void checkEmitterData(JPAEmitterData* data, const RawEmitter& raw, uint16_t numTextures,
                      String& tags) {
    sCtx.block = "header";
    eqU("user index", data->getUserIndex(), raw.resId);
    eqU("infoNum", data->infoNum, 1);
    JPADataBlockLinkInfo* info = data->getLinkInfo()[0];
    eqU("keyNum", info->getKeyNum(), raw.keys);
    eqU("fldNum", info->getFieldNum(), raw.fields);
    eqU("mTextureNum", info->getTextureNum(), raw.textures);
    uint32_t keys = 0, fields = 0;
    bool seen[8] = {};
    for (const RawBlock& b : raw.blocks) {
        if (!tags.empty()) {
            tags += ",";
        }
        tags += tagText(b.tag);
        switch (b.tag) {
        case tag('B', 'E', 'M', '1'):
            seen[0] = true;
            if (info->getDynamics() == nullptr) {
                fail("emitter %d: no dynamics block", sCtx.emitter);
            } else {
                checkDynamics(info->getDynamics(), b.data());
            }
            break;
        case tag('F', 'L', 'D', '1'):
            if (fields >= raw.fields || info->getField()[fields] == nullptr) {
                fail("emitter %d: field block %u missing", sCtx.emitter, fields);
            } else {
                checkField(info->getField()[fields], b.data());
            }
            fields++;
            break;
        case tag('K', 'F', 'A', '1'):
            if (keys >= raw.keys || info->getKey()[keys] == nullptr) {
                fail("emitter %d: key block %u missing", sCtx.emitter, keys);
            } else {
                checkKey(info->getKey()[keys], b);
            }
            keys++;
            break;
        case tag('B', 'S', 'P', '1'):
            seen[1] = true;
            if (info->getBaseShape() == nullptr) {
                fail("emitter %d: no base shape", sCtx.emitter);
            } else {
                checkBaseShape(info->getBaseShape(), b);
            }
            break;
        case tag('E', 'S', 'P', '1'):
            seen[2] = true;
            if (info->getExtraShape() == nullptr) {
                fail("emitter %d: no extra shape", sCtx.emitter);
            } else {
                checkExtraShape(info->getExtraShape(), b.data());
            }
            break;
        case tag('S', 'S', 'P', '1'):
            seen[3] = true;
            if (info->getSweepShape() == nullptr) {
                fail("emitter %d: no sweep shape", sCtx.emitter);
            } else {
                checkSweepShape(info->getSweepShape(), b.data());
            }
            break;
        case tag('E', 'T', 'X', '1'):
            seen[4] = true;
            if (info->getExTexShape() == nullptr) {
                fail("emitter %d: no ex-tex shape", sCtx.emitter);
            } else {
                checkExTexShape(info->getExTexShape(), b.data());
            }
            break;
        case tag('T', 'D', 'B', '1'): {
            seen[5] = true;
            sCtx.block = "TDB1";
            const auto* idx = info->getTextureDataBase();
            if ((const void*)idx != (const void*)b.data()) {
                fail("emitter %d TDB1: table at %p, the block's data at %p", sCtx.emitter,
                     (const void*)idx, (const void*)b.data());
                break;
            }
            for (uint32_t i = 0; i < raw.textures && 0xC + i * 2 + 2 <= b.size; i++) {
                eqU("texture index", idx[i], rd16(b.data() + i * 2));
                if (rd16(b.data() + i * 2) >= numTextures) {
                    fail("emitter %d TDB1: index %u is texture %u of %u", sCtx.emitter, i,
                         rd16(b.data() + i * 2), numTextures);
                }
            }
            break;
        }
        default:
            fail("emitter %d: unknown block %s", sCtx.emitter, tagText(b.tag).c_str());
            break;
        }
    }
    eqU("FLD1 blocks", fields, raw.fields);
    eqU("KFA1 blocks", keys, raw.keys);
    // A block the file does not have stays NULL.
    if (!seen[1] && info->getBaseShape() != nullptr) fail("emitter %d: stray base shape", sCtx.emitter);
    if (!seen[2] && info->getExtraShape() != nullptr) fail("emitter %d: stray extra shape", sCtx.emitter);
    if (!seen[3] && info->getSweepShape() != nullptr) fail("emitter %d: stray sweep shape", sCtx.emitter);
    if (!seen[4] && info->getExTexShape() != nullptr) fail("emitter %d: stray ex-tex shape", sCtx.emitter);
    if (!seen[5] && info->getTextureDataBase() != nullptr) fail("emitter %d: stray TDB1", sCtx.emitter);
}

// ---- running emitters ------------------------------------------------------------------------

struct RunTotals {
    uint32_t emitters = 0;      // created
    uint32_t emitting = 0;      // produced at least one particle in the 30 frames
    uint32_t ended = 0;         // deleted by the manager before frame 30
    uint32_t frames = 0;        // emitter frames calculated
    uint64_t particleFrames = 0; // live particles summed over the frames
    uint32_t maxParticles = 0;
};

bool finite3(const JGeometry::TVec3<f32>& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

int checkParticles(JSUList<JPABaseParticle>* list, int frame, const char* which) {
    int n = 0;
    for (JSULink<JPABaseParticle>* link = list->getFirst(); link != nullptr; link = link->getNext()) {
        JPABaseParticle* p = link->getObject();
        if (!finite3(p->mGlobalPosition) || !finite3(p->mLocalPosition) || !finite3(p->mVelocity) ||
            !std::isfinite(p->mCurFrame) || !std::isfinite(p->mLifeTime)) {
            fail("emitter %d frame %d: %s particle not finite: pos (%g %g %g) vel (%g %g %g) "
                 "age %g life %g",
                 sCtx.emitter, frame, which, p->mGlobalPosition.x, p->mGlobalPosition.y,
                 p->mGlobalPosition.z, p->mVelocity.x, p->mVelocity.y, p->mVelocity.z, p->mCurFrame,
                 p->mLifeTime);
            return n;
        }
        n++;
    }
    return n;
}

void runEmitter(JPAEmitterManager* mgr, uint8_t rmId, const RawEmitter& raw, RunTotals& t) {
    sCtx.block = "emitter";
    constexpr u8 kGroup = 0;
    JGeometry::TVec3<f32> pos(0.0f, 0.0f, 0.0f);
    JPABaseEmitter* emtr = mgr->createSimpleEmitterID(pos, raw.resId, kGroup, rmId, NULL, NULL);
    if (emtr == nullptr) {
        fail("emitter %d (user index %u): createSimpleEmitterID returned NULL", sCtx.emitter,
             raw.resId);
        return;
    }
    t.emitters++;
    // JPABaseEmitter::create's copy of BEM1.
    const uint8_t* d = nullptr;
    for (const RawBlock& b : raw.blocks) {
        if (b.tag == tag('B', 'E', 'M', '1')) {
            d = b.data();
        }
    }
    if (d != nullptr) {
        // What JPABaseEmitter::create copied out of BEM1 (the fields the class exposes).
        sCtx.block = "create";
        eqU("checkEmDataFlag", emtr->checkEmDataFlag(0xFFFFFFFF), rd32(d));
        eqF("getRate", emtr->getRate(), d + 0x10);
    }
    sCtx.block = "calc";
    bool emitted = false;
    for (int frame = 0; frame < kCalcFrames; frame++) {
        mgr->calc(kGroup);
        if (mgr->mEmtrGroup[kGroup].getNumLinks() == 0) {
            t.ended++;
            break;
        }
        t.frames++;
        int n = checkParticles(emtr->getParticleList(), frame, "parent");
        n += checkParticles(emtr->getChildParticleList(), frame, "child");
        if (n != emtr->getParticleNumber()) {
            break; // a non-finite particle, already reported
        }
        t.particleFrames += n;
        if ((uint32_t)n > t.maxParticles) {
            t.maxParticles = n;
        }
        emitted = emitted || n > 0;
    }
    if (emitted) {
        t.emitting++;
    }
    mgr->forceDeleteAllEmitter();
    if (mgr->getEmitterNumber() != 0 || mgr->getParticleNumber() != 0) {
        fail("after forceDeleteAllEmitter %u emitters and %u particles are still in use",
             mgr->getEmitterNumber(), mgr->getParticleNumber());
    }
}

// ---- one file ----------------------------------------------------------------------------------

struct FileResult {
    uint32_t emitters = 0, textures = 0, keyValues = 0;
    RunTotals run;
};

void sweepFile(const String& path, JKRExpHeap* sweepHeap, JPAEmitterManager*& mgr,
               JPAResourceManager*& common, int fd, FileResult& r) {
    sWhere = path.c_str();
    sCtx = {-1, ""};
    bool isCommon = path == "/res/Particle/common.jpc";
    // The file bytes live in the sweep heap, loaded as mDoDvdThd_toMainRam_c::execute does (its
    // size is the heap block's).
    const uint8_t* bytes = (const uint8_t*)JKRDvdToMainRam(
        path.c_str(), NULL, EXPAND_SWITCH_UNKNOWN1, 0, sweepHeap,
        JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, NULL);
    int size = bytes != nullptr ? sweepHeap->getSize((void*)bytes) : 0;
    if (bytes == nullptr || size <= 0) {
        fail("JKRDvdToMainRam failed");
        return;
    }
    RawJpc raw;
    if (!parseRaw(bytes, (uint32_t)size, raw)) {
        JKRFree((void*)bytes);
        return;
    }
    JKRSolidHeap* sceneHeap = nullptr;
    JKRHeap* heap = sweepHeap;
    if (!isCommon) {
        // createRoomScene: a solid heap of all that is free (mDoExt_createSolidHeapFromGame(0,
        // 0)), adjusted after the load.
        sceneHeap = JKRSolidHeap::create((u32)-1, sweepHeap, false);
        if (sceneHeap == nullptr) {
            fail("no scene heap (sweep heap free %d)", (int)sweepHeap->getTotalFreeSize());
            JKRFree((void*)bytes);
            return;
        }
        heap = sceneHeap;
    }
    JPAResourceManager* res = new (heap, 0) JPAResourceManager(bytes, heap);
    if (sceneHeap != nullptr) {
        mDoExt_adjustSolidHeap(sceneHeap);
    }
    if (res == nullptr || res->getEmitterResource() == nullptr || res->getTextureResource() == nullptr) {
        fail("JPAResourceManager made nothing");
        return;
    }
    JPAEmitterResource* emtrRes = res->getEmitterResource();
    JPATextureResource* texRes = res->getTextureResource();
    eqU("registered emitters", emtrRes->registNum, (uint32_t)raw.emitters.size());
    eqU("emitter slots", emtrRes->maxNum, (uint32_t)raw.emitters.size());
    eqU("registered textures", texRes->registNum, (uint32_t)raw.textures.size());
    eqU("texture slots", texRes->maxNum, (uint32_t)raw.textures.size());
    if (fd >= 0) {
        writef(fd, "JPC %s emitter_count=%u texture_count=%u\n", path.c_str(),
               (unsigned)emtrRes->registNum, (unsigned)texRes->registNum);
    }
    uint32_t nEmitters = emtrRes->registNum < raw.emitters.size() ? emtrRes->registNum
                                                                 : (uint32_t)raw.emitters.size();
    for (uint32_t i = 0; i < nEmitters; i++) {
        sCtx = {(int)i, "header"};
        JPAEmitterData* data = emtrRes->pEmtrResArray[i];
        String tags;
        checkEmitterData(data, raw.emitters[i], (uint16_t)raw.textures.size(), tags);
        JPADataBlockLinkInfo* info = data->getLinkInfo()[0];
        uint32_t blocks = (uint32_t)raw.emitters[i].blocks.size();
        for (const RawBlock& b : raw.emitters[i].blocks) {
            if (b.tag == tag('K', 'F', 'A', '1')) {
                r.keyValues += (uint32_t)b.data()[4] * 4;
            }
        }
        if (fd >= 0) {
            writef(fd, "EMTR %s %u res_id=%u blocks=%u keys=%u fields=%u textures=%u tags=%s\n",
                   path.c_str(), (unsigned)i, (unsigned)data->getUserIndex(), (unsigned)blocks,
                   (unsigned)info->getKeyNum(), (unsigned)info->getFieldNum(),
                   (unsigned)info->getTextureNum(), tags.c_str());
        }
    }
    r.emitters += nEmitters;
    uint32_t nTextures = texRes->registNum < raw.textures.size() ? texRes->registNum
                                                                 : (uint32_t)raw.textures.size();
    for (uint32_t i = 0; i < nTextures; i++) {
        sCtx = {-1, "TEX1"};
        JPATexture* tex = texRes->pTexResArray[i];
        const RawTexture& rt = raw.textures[i];
        const uint8_t* timg = rt.base + 0x20;
        if (rt.name != tex->getName()) {
            fail("texture %u: name %s, the file has %s", i, tex->getName(), rt.name.c_str());
        }
        JUTTexture* jut = tex->getJUTTexture();
        if ((const uint8_t*)jut->getTexInfo() != timg) {
            fail("texture %u: ResTIMG at %p, the block's at %p", i, (const void*)jut->getTexInfo(),
                 (const void*)timg);
        }
        eqU("texture format", jut->getFormat(), timg[0]);
        eqU("texture width", (uint32_t)jut->getWidth(), rd16(timg + 2));
        eqU("texture height", (uint32_t)jut->getHeight(), rd16(timg + 4));
        if (fd >= 0) {
            writef(fd, "TEX %s %u name=%s tex_format=%u width=%u height=%u mipmap_count=%u "
                       "image_offset=%u\n",
                   path.c_str(), (unsigned)i, tex->getName(), (unsigned)jut->getFormat(),
                   (unsigned)jut->getWidth(), (unsigned)jut->getHeight(),
                   (unsigned)jut->getTexInfo()->mipmapCount,
                   (unsigned)jut->getTexInfo()->imageOffset);
        }
    }
    r.textures += nTextures;

    // The emitters, as the game creates them: common.jpc is resource manager 0 of a manager with
    // dPa_control_c's pool sizes, each scene's is resource manager 1 (createRoomScene).
    uint8_t rmId = 0;
    if (isCommon) {
        common = res;
        mgr = new (sweepHeap, 0) JPAEmitterManager(res, 3000, 150, 200, sweepHeap);
    } else {
        if (mgr == nullptr) {
            fail("no emitter manager: common.jpc failed");
            return;
        }
        mgr->pResMgrArray[1] = res;
        rmId = 1;
    }
    for (uint32_t i = 0; i < nEmitters; i++) {
        sCtx = {(int)i, "emitter"};
        runEmitter(mgr, rmId, raw.emitters[i], r.run);
    }
    if (!isCommon) {
        // removeRoomScene
        mgr->clearResourceManager(1);
        mDoExt_destroySolidHeap(sceneHeap);
        JKRFree((void*)bytes);
    }
}

void findJpc(Vector<String>& out) {
    DVDDir dir;
    if (!DVDOpenDir("/res/Particle", &dir)) {
        fail("DVDOpenDir(/res/Particle) failed");
        return;
    }
    DVDDirEntry entry;
    Vector<String> scenes;
    bool common = false;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr || entry.isDir) {
            continue;
        }
        size_t len = strlen(entry.name);
        if (len > 4 && strcasecmp(entry.name + len - 4, ".jpc") == 0) {
            if (strcmp(entry.name, "common.jpc") == 0) {
                common = true;
            } else {
                scenes.push_back(String("/res/Particle/") + entry.name);
            }
        }
    }
    DVDCloseDir(&dir);
    // common.jpc first: the scenes' emitters run in its manager, as in the game.
    if (common) {
        out.push_back("/res/Particle/common.jpc");
    } else {
        fail("/res/Particle has no common.jpc");
    }
    out.insert(out.end(), scenes.begin(), scenes.end());
}

} // namespace

[[noreturn]] void smokeJpaSweep() {
    uint64_t start = elapsedMs();
    sWhere = "jpa-sweep";
    Vector<String> files;
    findJpc(files);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] jpa-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("jpa_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# jpa-sweep (COS_SMOKE=jpa-sweep): what the game's JParticle code read, in "
                   "disc_manifest.py's names\n");
    }
    JPAEmitterManager* mgr = nullptr;
    JPAResourceManager* common = nullptr;
    FileResult total;
    uint32_t done = 0;
    for (const String& path : files) {
        int before = sErrors;
        FileResult r;
        sweepFile(path, heap, mgr, common, fd, r);
        writef(STDERR_FILENO,
               "[cos] jpa-sweep: %s: %u emitters, %u textures, %u key values; %u emitters run "
               "(%u emitting, %u ended), %u frames, up to %u particles; %d error(s)\n",
               path.c_str(), r.emitters, r.textures, r.keyValues, r.run.emitters, r.run.emitting,
               r.run.ended, r.run.frames, r.run.maxParticles, sErrors - before);
        total.emitters += r.emitters;
        total.textures += r.textures;
        total.keyValues += r.keyValues;
        total.run.emitters += r.run.emitters;
        total.run.emitting += r.run.emitting;
        total.run.ended += r.run.ended;
        total.run.frames += r.run.frames;
        total.run.particleFrames += r.run.particleFrames;
        if (r.run.maxParticles > total.run.maxParticles) {
            total.run.maxParticles = r.run.maxParticles;
        }
        done++;
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "jpa-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] jpa-sweep: %u files, %u emitter resources, %u textures, %u key values; %u "
           "emitters calculated for up to %d frames (%u emitting, %u ended early), %u emitter "
           "frames, %llu particle frames, up to %u particles; %llu ms; %d error(s)%s\n",
           done, total.emitters, total.textures, total.keyValues, total.run.emitters, kCalcFrames,
           total.run.emitting, total.run.ended, total.run.frames,
           (unsigned long long)total.run.particleFrames, total.run.maxParticles,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in jpa_sweep.txt)" : "");
    bool pass = sErrors == 0 && done == files.size() && files.size() > 1 &&
                total.run.emitters == total.emitters && total.run.emitting > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
