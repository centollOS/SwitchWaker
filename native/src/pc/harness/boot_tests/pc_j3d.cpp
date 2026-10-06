// J3D model data on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.11): the COS_SMOKE=j3d-sweep
// test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc of the
// disc under /res (the FST walked with DVDOpenDir/DVDReadDir) is mounted in main RAM, as
// dRes_info_c mounts an object or stage archive, and every BMD, BDL and BMT in it (J3D2bmd2/bmd3,
// J3D2bdl4, J3D2bmt3, recognised by their magic) is loaded through J3DModelLoaderDataBase in a
// solid heap made as dRes_info_c::setRes makes one (mDoExt_createSolidHeapToCurrent, then
// adjusted), with the game's loader and flags:
// - a file in a directory dRes_info_c::loadResource reads (BMD, BMDM, BMDC, BMDS, BSMD, BDL, BDLM,
//   BDLC, BDLI, BMT, BMTM) gets that type's call and flags ("res");
// - a BDL under /res/Stage/sea/LODALL.arc is d_a_lod_bg's (loadBinaryDisplayList, 0x2020: "lod");
// - any other one (a BDL2 directory or a file at an archive's root, which no game code reads) is
//   loaded with its kind's dRes flags ("none": BMD 0x51240020, BDL 0x2020).
// The model data the game built is then compared with an independent reading of the file (plain
// big-endian loads at the format's offsets, no game struct):
// - INF1: the model flags (loader flags | file flags), vertex count, hierarchy address;
// - VTX1: the vertex attribute format list and every array's address;
// - EVP1: the weighted matrix count, each mix count, mix index and weight, every inverse joint
//   matrix the mix indices use; DRW1: the draw matrix count, flags, indices and full-weight count;
// - JNT1: joint count, names, and each joint's kind, scale compensation, transform and bounds;
// - SHP1: shape count, and each shape's matrix group count, bounds, vertex descriptor list, and
//   per group its display list (address and size) and used matrix indices;
// - MAT3/MAT2 (BMD, BMT, the patched BDL materials): material count and names, and each
//   material's cull mode, colour channel, tex-gen and TEV stage counts, material colours, TEV
//   colours, texture numbers, tex matrices, fog and NBT scale (where its blocks keep them);
// - MDL3 (BDL): each material's display list (address and size), current matrix registers and
//   patching offsets;
// - TEX1: texture count, names and ResTIMG table address.
// <COS_RUN_DIR>/j3d_sweep.txt gets what the game read (J3D/JNT1/MAT/SHP1/TEX1/DRW1/EVP1/INF1
// lines); native/tools/run.sh compares it with the manifest (disc_manifest.py --check-j3d):
// every model of the disc, its joint, material, shape, texture, draw and envelope counts and its
// joint, material and texture names.
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DMaterialAttach.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DMatBlock.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DShapeDraw.h"
#include "JSystem/J3DGraphBase/J3DShapeMtx.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "m_Do/m_Do_ext.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 48 * 1024 * 1024;

int sErrors = 0;
const char* sWhere = "";

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] j3d-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int16_t rds16(const uint8_t* p) {
    return (int16_t)rd16(p);
}

f32 rdf(const uint8_t* p) {
    uint32_t bits = rd32(p);
    f32 v;
    memcpy(&v, &bits, 4);
    return v;
}

uint32_t bitsOf(f32 v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    return bits;
}

// Floats are compared bit for bit: the game only copies them.
bool sameF(f32 got, const uint8_t* raw) {
    return bitsOf(got) == rd32(raw);
}

// A name for the report: printable ASCII but space, comma and '%' kept, the rest as %XX.
String enc(const char* text) {
    String out;
    for (const char* c = text; *c != '\0'; c++) {
        unsigned char ch = (unsigned char)*c;
        if (ch > 0x20 && ch < 0x7F && ch != ',' && ch != '%') {
            out += (char)ch;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", ch);
            out += hex;
        }
    }
    return out;
}

// ResNTAB, read independently: u16 count, pad, {u16 key, u16 offset} entries, names from the table.
bool rawNames(const uint8_t* tab, const uint8_t* end, Vector<String>& out) {
    if (tab + 4 > end) {
        return false;
    }
    uint16_t n = rd16(tab);
    if (tab + 4 + n * 4 > end) {
        return false;
    }
    for (uint16_t i = 0; i < n; i++) {
        const char* name = (const char*)tab + rd16(tab + 4 + i * 4 + 2);
        if ((const uint8_t*)name >= end || memchr(name, 0, end - (const uint8_t*)name) == nullptr) {
            return false;
        }
        out.push_back(name);
    }
    return true;
}

// The game's name table against the file's; returns the names joined for the report.
String checkNames(const char* what, const JUTNameTab* tab, const uint8_t* rawTab, uint32_t num,
                  const uint8_t* end) {
    if (rawTab == nullptr) {
        if (tab != nullptr) {
            fail("%s: a name table the file does not have", what);
        }
        return "";
    }
    Vector<String> want;
    if (!rawNames(rawTab, end, want)) {
        fail("%s: the file's name table is malformed", what);
        return "";
    }
    if (tab == nullptr) {
        fail("%s: no name table; the file has %zu names", what, want.size());
        return "";
    }
    if ((const uint8_t*)tab->getResNameTable() != rawTab || want.size() != num) {
        fail("%s: name table at %p for %u entries; the file has %zu names at %p", what,
             (const void*)tab->getResNameTable(), num, want.size(), (const void*)rawTab);
        return "";
    }
    String joined;
    for (uint32_t i = 0; i < num; i++) {
        const char* got = tab->getName((u16)i);
        if (got == nullptr || want[i] != got) {
            fail("%s %u: name %s, the file has %s", what, i, got != nullptr ? got : "(null)",
                 want[i].c_str());
        }
        if (i != 0) {
            joined += ",";
        }
        joined += enc(want[i].c_str());
    }
    return joined;
}

// A report line with a name list, which can be longer than writef's buffer.
void writeNamesLine(int fd, const char* kind, const String& path, unsigned int count,
                    const String& names) {
    char head[64];
    snprintf(head, sizeof(head), " count=%u names=", count);
    String line = String(kind) + " " + path + head + names + "\n";
    const char* p = line.c_str();
    size_t left = line.size();
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) {
            break;
        }
        p += w;
        left -= (size_t)w;
    }
}

// ---- the independent reading -------------------------------------------------------------------

struct RawBlock {
    char tag[5];
    const uint8_t* p;
    uint32_t size;
};

struct RawFile {
    const uint8_t* base;
    uint32_t size;
    char magic[9];
    uint32_t blockCount;
    Vector<RawBlock> blocks;
    const RawBlock* find(const char* tag) const {
        for (const RawBlock& b : blocks) {
            if (memcmp(b.tag, tag, 4) == 0) {
                return &b;
            }
        }
        return nullptr;
    }
    // A table at offset `off` from block b; null for offset 0 (as JSUConvertOffsetToPtr).
    const uint8_t* at(const RawBlock* b, uint32_t field) const {
        uint32_t off = rd32(b->p + field);
        return off != 0 ? b->p + off : nullptr;
    }
};

bool parseRaw(const uint8_t* bytes, uint32_t size, RawFile& raw) {
    raw.base = bytes;
    raw.size = size;
    if (size < 0x20) {
        return false;
    }
    memcpy(raw.magic, bytes, 8);
    raw.magic[8] = 0;
    uint32_t declared = rd32(bytes + 8);
    raw.blockCount = rd32(bytes + 0xC);
    if (declared < size) {
        raw.size = declared;
    }
    // As disc_manifest.py: stop at the end (a few BMT declare a block they do not hold).
    uint32_t o = 0x20;
    for (uint32_t k = 0; k < raw.blockCount && o + 8 <= raw.size; k++) {
        RawBlock b;
        memcpy(b.tag, bytes + o, 4);
        b.tag[4] = 0;
        b.p = bytes + o;
        b.size = rd32(bytes + o + 4);
        if (b.size < 8) {
            return false;
        }
        raw.blocks.push_back(b);
        o += b.size;
    }
    return true;
}

// ---- the loads -----------------------------------------------------------------------------------

enum LoaderKind { kBmd, kBdl, kBmt };

struct LoadPlan {
    LoaderKind kind;
    uint32_t flags;      // the loader's flags (unused for BMT)
    const char* source;  // res, lod, none
};

// dRes_info_c::loadResource's calls per directory type (d_resorce.cpp), and 'BDLL'.
bool planFor(uint32_t dirType, LoaderKind fileKind, LoadPlan& plan) {
    static const struct {
        uint32_t type;
        LoaderKind kind;
        uint32_t flags;
    } kRes[] = {
        {'BMD ', kBmd, 0x51240020}, {'BMDM', kBmd, 0x51240020}, {'BMDC', kBmd, 0x51240020},
        {'BMDS', kBmd, 0x00220020}, {'BSMD', kBmd, 0x01020020}, {'BDL ', kBdl, 0x00002020},
        {'BDLL', kBdl, 0x00001020}, {'BDLM', kBdl, 0x00002020}, {'BDLI', kBdl, 0x01002020},
        {'BDLC', kBdl, 0x00002020}, {'BMT ', kBmt, 0},          {'BMTM', kBmt, 0},
    };
    for (const auto& r : kRes) {
        if (r.type == dirType) {
            plan = {r.kind, r.flags, "res"};
            return r.kind == fileKind;
        }
    }
    static const uint32_t kDefault[] = {0x51240020, 0x00002020, 0};
    plan = {fileKind, kDefault[fileKind], "none"};
    return true;
}

struct Totals {
    unsigned int archives = 0;
    unsigned int models[3] = {0, 0, 0};
    unsigned int joints = 0;
    unsigned int materials = 0;
    unsigned int shapes = 0;
    unsigned int textures = 0;
    unsigned int drawMtx = 0;
    unsigned int envelopes = 0;
    unsigned int v21 = 0;
};

Totals sTotals;

// ---- the checks ----------------------------------------------------------------------------------

void checkInfo(J3DModelData* md, const RawFile& raw, const LoadPlan& plan, int fd,
               const String& path) {
    const RawBlock* b = raw.find("INF1");
    if (b == nullptr) {
        fail("no INF1 block");
        return;
    }
    uint32_t flags = plan.flags | rd16(b->p + 8);
    if (md->getFlag() != flags) {
        fail("model flags 0x%x, want 0x%x", (unsigned)md->getFlag(), flags);
    }
    if (md->getVtxNum() != rd32(b->p + 0x10)) {
        fail("vertex count %u, the file has %u", (unsigned)md->getVtxNum(), rd32(b->p + 0x10));
    }
    if ((const uint8_t*)md->getHierarchy() != raw.at(b, 0x14)) {
        fail("hierarchy at %p, the file's at %p", (const void*)md->getHierarchy(),
             (const void*)raw.at(b, 0x14));
    }
    if (fd >= 0) {
        writef(fd, "INF1 %s flags=%u vertices=%u\n", path.c_str(), (unsigned)(md->getFlag() & ~plan.flags),
               (unsigned)md->getVtxNum());
    }
}

void checkVertex(J3DModelData* md, const RawFile& raw) {
    const RawBlock* b = raw.find("VTX1");
    if (b == nullptr) {
        fail("no VTX1 block");
        return;
    }
    J3DVertexData& vd = md->getVertexData();
    const uint8_t* fmt = raw.at(b, 0x08);
    GXVtxAttrFmtList* list = vd.getVtxAttrFmtList();
    if (fmt == nullptr || list == nullptr) {
        fail("vertex format list %p, the file's at %p", (void*)list, (const void*)fmt);
    } else {
        for (int i = 0;; i++, fmt += 0x10) {
            if (fmt + 0x10 > b->p + b->size) {
                fail("vertex format list runs past VTX1");
                break;
            }
            if ((uint32_t)list[i].attr != rd32(fmt) || (uint32_t)list[i].cnt != rd32(fmt + 4) ||
                (uint32_t)list[i].type != rd32(fmt + 8) || list[i].frac != fmt[12]) {
                fail("vertex format %d: attr %u cnt %u type %u frac %u, the file has %u %u %u %u",
                     i, (unsigned)list[i].attr, (unsigned)list[i].cnt, (unsigned)list[i].type,
                     (unsigned)list[i].frac, rd32(fmt), rd32(fmt + 4), rd32(fmt + 8), fmt[12]);
                break;
            }
            if (rd32(fmt) == GX_VA_NULL) {
                break;
            }
        }
    }
    const void* got[13] = {vd.getVtxPosArray(), vd.getVtxNrmArray(), vd.getVtxNBTArray(),
                           vd.getVtxColorArray(0), vd.getVtxColorArray(1)};
    for (int i = 0; i < 8; i++) {
        got[5 + i] = vd.getVtxTexCoordArray((u8)i);
    }
    static const uint32_t kField[13] = {0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
                                        0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C};
    for (int i = 0; i < 13; i++) {
        if (got[i] != raw.at(b, kField[i])) {
            fail("vertex array %d at %p, the file's at %p", i, got[i], (const void*)raw.at(b, kField[i]));
        }
    }
}

void checkEnvelope(J3DModelData* md, const RawFile& raw, int fd, const String& path) {
    const RawBlock* b = raw.find("EVP1");
    if (b == nullptr) {
        return;
    }
    uint16_t num = rd16(b->p + 8);
    J3DJointTree& tree = md->getJointTree();
    if (tree.getWEvlpMtxNum() != num) {
        fail("EVP1: %u weighted matrices, the file has %u", tree.getWEvlpMtxNum(), num);
        return;
    }
    const uint8_t* mixNum = raw.at(b, 0x0C);
    const uint8_t* mixIndex = raw.at(b, 0x10);
    const uint8_t* mixWeight = raw.at(b, 0x14);
    const uint8_t* inv = raw.at(b, 0x18);
    uint32_t k = 0;
    for (uint16_t i = 0; i < num && mixNum != nullptr; i++) {
        if (tree.getWEvlpMixMtxNum(i) != mixNum[i]) {
            fail("EVP1 %u: %u mixed matrices, the file has %u", i, tree.getWEvlpMixMtxNum(i),
                 mixNum[i]);
            return;
        }
        for (uint8_t j = 0; j < mixNum[i]; j++, k++) {
            uint16_t idx = rd16(mixIndex + k * 2);
            if (tree.getWEvlpMixMtxIndex()[k] != idx || !sameF(tree.getWEvlpMixWeight()[k], mixWeight + k * 4)) {
                fail("EVP1 mix %u: index %u weight %g, the file has %u %g", k,
                     tree.getWEvlpMixMtxIndex()[k], tree.getWEvlpMixWeight()[k], idx,
                     rdf(mixWeight + k * 4));
                return;
            }
            const f32(*m)[4] = tree.getInvJointMtx(idx);
            for (int e = 0; e < 12; e++) {
                if (!sameF(m[e / 4][e % 4], inv + idx * 48 + e * 4)) {
                    fail("EVP1 inverse matrix %u [%d][%d]: %g, the file has %g", idx, e / 4, e % 4,
                         m[e / 4][e % 4], rdf(inv + idx * 48 + e * 4));
                    return;
                }
            }
        }
    }
    sTotals.envelopes += num;
    if (fd >= 0) {
        writef(fd, "EVP1 %s count=%u\n", path.c_str(), num);
    }
}

void checkDraw(J3DModelData* md, const RawFile& raw, int fd, const String& path) {
    const RawBlock* b = raw.find("DRW1");
    if (b == nullptr) {
        return;
    }
    uint16_t num = rd16(b->p + 8);
    if (md->getDrawMtxNum() != num) {
        fail("DRW1: %u draw matrices, the file has %u", md->getDrawMtxNum(), num);
        return;
    }
    const uint8_t* flag = raw.at(b, 0x0C);
    const uint8_t* index = raw.at(b, 0x10);
    uint16_t full = num;
    for (uint16_t i = 0; i < num; i++) {
        if (md->getDrawMtxFlag(i) != flag[i] || md->getDrawMtxIndex(i) != rd16(index + i * 2)) {
            fail("DRW1 %u: flag %u index %u, the file has %u %u", i, md->getDrawMtxFlag(i),
                 md->getDrawMtxIndex(i), flag[i], rd16(index + i * 2));
            return;
        }
        if (flag[i] == 1 && full == num) {
            full = i;
        }
    }
    if (md->getDrawFullWgtMtxNum() != full) {
        fail("DRW1: %u full-weight matrices, the file has %u", md->getDrawFullWgtMtxNum(), full);
    }
    sTotals.drawMtx += num;
    if (fd >= 0) {
        writef(fd, "DRW1 %s count=%u\n", path.c_str(), num);
    }
}

void checkJoints(J3DModelData* md, const RawFile& raw, int fd, const String& path) {
    const RawBlock* b = raw.find("JNT1");
    if (b == nullptr) {
        fail("no JNT1 block");
        return;
    }
    uint16_t num = rd16(b->p + 8);
    if (md->getJointNum() != num) {
        fail("JNT1: %u joints, the file has %u", md->getJointNum(), num);
        return;
    }
    const uint8_t* init = raw.at(b, 0x0C);
    const uint8_t* index = raw.at(b, 0x10);
    for (uint16_t i = 0; i < num; i++) {
        J3DJoint* joint = md->getJointNodePointer(i);
        const uint8_t* d = init + rd16(index + i * 2) * 0x40;
        uint8_t scaleComp = d[2] == 0xFF ? 0 : d[2];
        J3DTransformInfo& t = joint->getTransformInfo();
        bool same = joint->getJntNo() == i && joint->getKind() == (rd16(d) & 0x0F) &&
                    joint->getScaleCompensate() == scaleComp && sameF(t.mScale.x, d + 0x04) &&
                    sameF(t.mScale.y, d + 0x08) && sameF(t.mScale.z, d + 0x0C) &&
                    t.mRotation.x == rds16(d + 0x10) && t.mRotation.y == rds16(d + 0x12) &&
                    t.mRotation.z == rds16(d + 0x14) && sameF(t.mTranslate.x, d + 0x18) &&
                    sameF(t.mTranslate.y, d + 0x1C) && sameF(t.mTranslate.z, d + 0x20) &&
                    sameF(joint->getMin()->x, d + 0x28) && sameF(joint->getMin()->y, d + 0x2C) &&
                    sameF(joint->getMin()->z, d + 0x30) && sameF(joint->getMax()->x, d + 0x34) &&
                    sameF(joint->getMax()->y, d + 0x38) && sameF(joint->getMax()->z, d + 0x3C);
        if (!same) {
            fail("joint %u (number %u): kind %u, scale compensation %u, scale (%g %g %g), rotation "
                 "(%d %d %d), translation (%g %g %g), bounds (%g %g %g)..(%g %g %g) differ from "
                 "the file's kind %u, scale compensation 0x%x, scale (%g %g %g), rotation (%d %d "
                 "%d), translation (%g %g %g), bounds (%g %g %g)..(%g %g %g)",
                 i, joint->getJntNo(), joint->getKind(), joint->getScaleCompensate(), t.mScale.x,
                 t.mScale.y, t.mScale.z, t.mRotation.x, t.mRotation.y, t.mRotation.z,
                 t.mTranslate.x, t.mTranslate.y, t.mTranslate.z, joint->getMin()->x,
                 joint->getMin()->y, joint->getMin()->z, joint->getMax()->x, joint->getMax()->y,
                 joint->getMax()->z, rd16(d), d[2], rdf(d + 4), rdf(d + 8), rdf(d + 0xC),
                 rds16(d + 0x10), rds16(d + 0x12), rds16(d + 0x14), rdf(d + 0x18), rdf(d + 0x1C),
                 rdf(d + 0x20), rdf(d + 0x28), rdf(d + 0x2C), rdf(d + 0x30), rdf(d + 0x34),
                 rdf(d + 0x38), rdf(d + 0x3C));
            return;
        }
    }
    String names = checkNames("joint", md->getJointName(), raw.at(b, 0x14), num, b->p + b->size);
    sTotals.joints += num;
    if (fd >= 0) {
        writeNamesLine(fd, "JNT1", path, num, names);
    }
}

void checkShapes(J3DModelData* md, const RawFile& raw, int fd, const String& path) {
    const RawBlock* b = raw.find("SHP1");
    if (b == nullptr) {
        fail("no SHP1 block");
        return;
    }
    uint16_t num = rd16(b->p + 8);
    if (md->getShapeNum() != num) {
        fail("SHP1: %u shapes, the file has %u", md->getShapeNum(), num);
        return;
    }
    const uint8_t* init = raw.at(b, 0x0C);
    const uint8_t* index = raw.at(b, 0x10);
    const uint8_t* desc = raw.at(b, 0x18);
    const uint8_t* mtxTable = raw.at(b, 0x1C);
    const uint8_t* dl = raw.at(b, 0x20);
    const uint8_t* mtxInit = raw.at(b, 0x24);
    const uint8_t* drawInit = raw.at(b, 0x28);
    for (uint16_t i = 0; i < num; i++) {
        J3DShape* shape = md->getShapeNodePointer(i);
        if (shape == nullptr) {
            fail("shape %u: none (not in the hierarchy)", i);
            continue;
        }
        const uint8_t* d = init + rd16(index + i * 2) * 0x28;
        uint16_t groups = rd16(d + 2);
        if (shape->getIndex() != i || shape->getMtxGroupNum() != groups ||
            !sameF(shape->getMin()->x, d + 0x10) || !sameF(shape->getMin()->y, d + 0x14) ||
            !sameF(shape->getMin()->z, d + 0x18) || !sameF(shape->getMax()->x, d + 0x1C) ||
            !sameF(shape->getMax()->y, d + 0x20) || !sameF(shape->getMax()->z, d + 0x24)) {
            fail("shape %u: index %u, %u groups, bounds (%g %g %g)..(%g %g %g); the file has %u "
                 "groups, (%g %g %g)..(%g %g %g)",
                 i, (unsigned)shape->getIndex(), (unsigned)shape->getMtxGroupNum(),
                 shape->getMin()->x, shape->getMin()->y, shape->getMin()->z, shape->getMax()->x,
                 shape->getMax()->y, shape->getMax()->z, groups, rdf(d + 0x10), rdf(d + 0x14),
                 rdf(d + 0x18), rdf(d + 0x1C), rdf(d + 0x20), rdf(d + 0x24));
            continue;
        }
        const uint8_t* rd = desc + rd16(d + 4);
        GXVtxDescList* vd = shape->getVtxDesc();
        for (int k = 0;; k++, rd += 8) {
            if (vd == nullptr || (uint32_t)vd[k].attr != rd32(rd) || (uint32_t)vd[k].type != rd32(rd + 4)) {
                fail("shape %u: vertex descriptor %d differs from the file's (%u %u)", i, k,
                     rd32(rd), rd32(rd + 4));
                break;
            }
            if (rd32(rd) == GX_VA_NULL) {
                break;
            }
        }
        for (uint16_t g = 0; g < groups; g++) {
            const uint8_t* di = drawInit + (rd16(d + 8) + g) * 8;
            J3DShapeDraw* draw = shape->getShapeDraw(g);
            if (draw == nullptr || draw->getDisplayList() != dl + rd32(di + 4) ||
                draw->getDisplayListSize() != rd32(di)) {
                fail("shape %u group %u: display list differs from the file's (0x%x bytes at 0x%x)",
                     i, g, rd32(di), rd32(di + 4));
                break;
            }
            const uint8_t* mi = mtxInit + (rd16(d + 6) + g) * 8;
            J3DShapeMtx* mtx = shape->getShapeMtx(g);
            uint16_t useNum = rd16(mi + 2);
            uint32_t first = rd32(mi + 4);
            bool multi = mtx != nullptr && mtx->getUseMtxNum() != 1;
            bool ok = mtx != nullptr && mtx->getUseMtxIndex(0) == (multi ? rd16(mtxTable + first * 2) : rd16(mi));
            if (ok && multi) {
                ok = mtx->getUseMtxNum() == useNum;
                for (uint16_t u = 0; ok && u < useNum; u++) {
                    ok = mtx->getUseMtxIndex(u) == rd16(mtxTable + (first + u) * 2);
                }
            }
            if (!ok) {
                fail("shape %u group %u: matrix %u (%u used), the file has %u (%u used from %u)",
                     i, g, mtx != nullptr ? mtx->getUseMtxIndex(0) : 0xFFFF,
                     mtx != nullptr ? (unsigned)mtx->getUseMtxNum() : 0u, rd16(mi), useNum, first);
                break;
            }
        }
    }
    sTotals.shapes += num;
    if (fd >= 0) {
        writef(fd, "SHP1 %s count=%u\n", path.c_str(), num);
    }
}

// The material block offsets of MAT3 (J3DMaterialBlock) and MAT2 (J3DMaterialBlock_v21), and the
// init data offsets of J3DMaterialInitData / J3DMaterialInitData_v21 the checks read.
struct MatLayout {
    uint32_t initSize;
    uint32_t cull, matColor, colorChanNum, texGenNum, texMtx, texNo, tevColor, tevStageNum, fog,
        nbt;
    uint32_t iMatColor, iTexMtx, iTexNo, iTevColor, iFog, iNbt;
};

const MatLayout kMat3 = {0x14C, 0x1C, 0x20, 0x24, 0x34, 0x40, 0x48, 0x50, 0x58, 0x68, 0x80,
                         0x08, 0x48, 0x84, 0xDC, 0x144, 0x14A};
const MatLayout kMat2 = {0x138, 0x18, 0x1C, 0x20, 0x28, 0x34, 0x3C, 0x44, 0x4C, 0x5C, 0x74,
                         0x08, 0x34, 0x70, 0xC8, 0x130, 0x136};

bool sameTexMtx(J3DTexMtx* m, const uint8_t* r) {
    J3DTexMtxInfo& info = m->getTexMtxInfo();
    if (info.mProjection != r[0] || info.mInfo != r[1] || !sameF(info.mCenter.x, r + 4) ||
        !sameF(info.mCenter.y, r + 8) || !sameF(info.mCenter.z, r + 0xC) ||
        !sameF(info.mSRT.mScaleX, r + 0x10) || !sameF(info.mSRT.mScaleY, r + 0x14) ||
        info.mSRT.mRotation != rds16(r + 0x18) || !sameF(info.mSRT.mTranslationX, r + 0x1C) ||
        !sameF(info.mSRT.mTranslationY, r + 0x20)) {
        return false;
    }
    for (int e = 0; e < 16; e++) {
        if (!sameF(info.mEffectMtx[e / 4][e % 4], r + 0x24 + e * 4)) {
            return false;
        }
    }
    return true;
}

bool sameFog(J3DFog* fog, const uint8_t* r) {
    J3DFogInfo* info = fog->getFogInfo();
    if (info->mType != r[0] || info->mAdjEnable != r[1] || info->mCenter != rd16(r + 2) ||
        !sameF(info->mStartZ, r + 4) || !sameF(info->mEndZ, r + 8) ||
        !sameF(info->mNearZ, r + 0xC) || !sameF(info->mFarZ, r + 0x10) ||
        memcmp(&info->mColor, r + 0x14, 4) != 0) {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        if (info->mFogAdjTable[i] != rd16(r + 0x18 + i * 2)) {
            return false;
        }
    }
    return true;
}

// One material made by J3DMaterialFactory(_v21)::create (normal, or patched for a BDL) against the
// file. A block that does not keep a value (a null block, the patched tex-gen block's count) is
// not compared for it.
void checkMaterial(J3DMaterial* mat, uint16_t i, const RawFile& raw, const RawBlock* b,
                   const MatLayout& L, bool patched) {
    const uint8_t* init = raw.at(b, 0x0C) + rd16(raw.at(b, 0x10) + i * 2) * L.initSize;
    auto table = [&](uint32_t field) { return raw.at(b, field); };
    J3DColorBlock* color = mat->getColorBlock();
    J3DTevBlock* tev = mat->getTevBlock();
    J3DTexGenBlock* texGen = mat->getTexGenBlock();
    J3DPEBlock* pe = mat->getPEBlock();
    if (mat->getIndex() != i) {
        fail("material %u: index %u", i, mat->getIndex());
    }
    if (color->getType() == 'CLNL' || tev->getType() == 'TVNL' || texGen->getType() == 'TGNL' ||
        pe->getType() == 'PENL') {
        fail("material %u: a null block", i);
        return;
    }
    uint8_t cull = init[1] != 0xFF ? (uint8_t)rd32(table(L.cull) + init[1] * 4) : 0xFF;
    if (color->getCullMode() != cull) {
        fail("material %u: cull mode %u, the file has %u", i, color->getCullMode(), cull);
    }
    uint8_t chans = init[2] != 0xFF ? table(L.colorChanNum)[init[2]] : 0;
    if (color->getColorChanNum() != chans) {
        fail("material %u: %u colour channels, the file has %u", i, color->getColorChanNum(), chans);
    }
    uint8_t texGens = init[3] != 0xFF ? table(L.texGenNum)[init[3]] : 0;
    if (!patched && texGen->getTexGenNum() != texGens) {
        fail("material %u: %u tex gens, the file has %u", i, (unsigned)texGen->getTexGenNum(),
             texGens);
    }
    uint8_t stages = init[4] != 0xFF ? table(L.tevStageNum)[init[4]] : 0xFF;
    if (tev->getTevStageNum() != stages) {
        fail("material %u: %u TEV stages, the file has %u", i, tev->getTevStageNum(), stages);
    }
    for (int k = 0; k < 2; k++) {
        uint16_t no = rd16(init + L.iMatColor + k * 2);
        J3DGXColor* c = color->getMatColor(k);
        if (no != 0xFFFF && c != nullptr && memcmp(&c->mColor, table(L.matColor) + no * 4, 4) != 0) {
            fail("material %u: material colour %d differs from the file's", i, k);
        }
    }
    for (int k = 0; k < 4; k++) {
        uint16_t no = rd16(init + L.iTevColor + k * 2);
        J3DGXColorS10* c = tev->getTevColor(k);
        if (no == 0xFFFF || c == nullptr) {
            continue;
        }
        const uint8_t* r = table(L.tevColor) + no * 8;
        if (c->mColor.r != rds16(r) || c->mColor.g != rds16(r + 2) || c->mColor.b != rds16(r + 4) ||
            c->mColor.a != rds16(r + 6)) {
            // The game's own patch: setToonTex (d_resorce.cpp) is not run here.
            fail("material %u: TEV colour %d (%d %d %d %d), the file has (%d %d %d %d)", i, k,
                 c->mColor.r, c->mColor.g, c->mColor.b, c->mColor.a, rds16(r), rds16(r + 2),
                 rds16(r + 4), rds16(r + 6));
        }
    }
    // The tex numbers the factory sets: all 8 for a patched material, else at least as many as the
    // file's TEV stages (the TEV block holds that many).
    uint32_t texNum = patched ? 8 : stages == 0xFF ? 0 : (stages < 8 ? stages : 8);
    for (uint32_t k = 0; k < texNum; k++) {
        uint16_t no = rd16(init + L.iTexNo + k * 2);
        uint16_t want = no != 0xFFFF ? rd16(table(L.texNo) + no * 2) : 0xFFFF;
        if (tev->getTexNo(k) != want) {
            fail("material %u: texture %u is %u, the file has %u", i, k, tev->getTexNo(k), want);
        }
    }
    for (int k = 0; k < 8; k++) {
        uint16_t no = rd16(init + L.iTexMtx + k * 2);
        J3DTexMtx* m = texGen->getTexMtx(k);
        if (m == nullptr) {
            continue;
        }
        if (no == 0xFFFF || !sameTexMtx(m, table(L.texMtx) + no * 0x64)) {
            fail("material %u: tex matrix %d differs from the file's (entry %u)", i, k, no);
        }
    }
    J3DFog* fog = pe->getFog();
    uint16_t fogNo = rd16(init + L.iFog);
    if (fog != nullptr && fogNo != 0xFFFF && !sameFog(fog, table(L.fog) + fogNo * 0x2C)) {
        J3DFogInfo* f = fog->getFogInfo();
        fail("material %u: fog type %u, z %g..%g, the file has %u, %g..%g", i, f->mType,
             f->mStartZ, f->mEndZ, table(L.fog)[fogNo * 0x2C], rdf(table(L.fog) + fogNo * 0x2C + 4),
             rdf(table(L.fog) + fogNo * 0x2C + 8));
    }
    J3DNBTScale* nbt = texGen->getNBTScale();
    uint16_t nbtNo = rd16(init + L.iNbt);
    if (nbt != nullptr && nbtNo != 0xFFFF) {
        const uint8_t* r = table(L.nbt) + nbtNo * 0x10;
        if (nbt->mbHasScale != r[0] || !sameF(nbt->mScale.x, r + 4) ||
            !sameF(nbt->mScale.y, r + 8) || !sameF(nbt->mScale.z, r + 0xC)) {
            fail("material %u: NBT scale differs from the file's", i);
        }
    }
}

// The current matrix registers J3DMaterialFactory::modifyPatchedCurrentMtx sets from MAT3 (BDL flag
// 0x2000): the tex-gen matrix of each of the material's tex coords, GX_IDENTITY past its count.
void patchedCurrentMtx(const RawFile& raw, const RawBlock* mat3, uint16_t i, uint32_t& regA,
                       uint32_t& regB) {
    const uint8_t* init = raw.at(mat3, 0x0C) + rd16(raw.at(mat3, 0x10) + i * 2) * kMat3.initSize;
    uint32_t texGens = init[3] != 0xFF ? raw.at(mat3, 0x34)[init[3]] : 0;
    uint32_t t[8];
    for (uint32_t k = 0; k < 8; k++) {
        uint16_t no = k < texGens ? rd16(init + 0x28 + k * 2) : 0xFFFF;
        t[k] = no != 0xFFFF ? raw.at(mat3, 0x38)[no * 4 + 2] : GX_IDENTITY;
    }
    regA = t[0] << 6 | t[1] << 12 | t[2] << 18 | t[3] << 24;
    regB = t[4] << 0 | t[5] << 6 | t[6] << 12 | t[7] << 18;
}

// MDL3: each locked material's display list, current matrix and patching offsets. With BDL flag
// 0x2000 the loader then recomputes the current matrix from MAT3 (modifyMaterial).
void checkMaterialDL(J3DMaterialTable& table, const RawFile& raw, const RawBlock* b,
                     const RawBlock* mat3, uint32_t flags) {
    uint16_t num = rd16(b->p + 8);
    const uint8_t* dlInit = raw.at(b, 0x0C);
    const uint8_t* patch = raw.at(b, 0x10);
    const uint8_t* cur = raw.at(b, 0x14);
    for (uint16_t i = 0; i < num && i < table.getMaterialNum(); i++) {
        J3DMaterial* mat = table.getMaterialNodePointer(i);
        const uint8_t* e = dlInit + i * 8;
        J3DDisplayListObj* dl = mat->getSharedDisplayListObj();
        if (dl == nullptr || (const uint8_t*)dl->getDisplayList(0) != e + rd32(e) ||
            dl->getDisplayListSize() != rd32(e + 4)) {
            fail("MDL3 material %u: display list differs from the file's (0x%x bytes at +0x%x)", i,
                 rd32(e + 4), rd32(e));
            return;
        }
        uint32_t regA = rd32(cur + i * 8);
        uint32_t regB = rd32(cur + i * 8 + 4);
        if ((flags & 0x2000) != 0 && mat3 != nullptr) {
            patchedCurrentMtx(raw, mat3, i, regA, regB);
        }
        if (mat->mCurrentMtx.getMtxIdxRegA() != regA || mat->mCurrentMtx.getMtxIdxRegB() != regB) {
            fail("MDL3 material %u: current matrix 0x%x 0x%x, the file gives 0x%x 0x%x", i,
                 (unsigned)mat->mCurrentMtx.getMtxIdxRegA(), (unsigned)mat->mCurrentMtx.getMtxIdxRegB(),
                 regA, regB);
            return;
        }
        // The patching offsets, where the material's blocks keep them (a locked material made
        // from MDL3 alone has null blocks, which keep none).
        const uint8_t* p = patch + i * 0x10;
        bool same = true;
        if (mat->getColorBlock()->getType() != 'CLNL') {
            same = same && mat->getColorBlock()->getMatColorOffset() == rd16(p) &&
                   mat->getColorBlock()->getColorChanOffset() == rd16(p + 2);
        }
        if (mat->getTexGenBlock()->getType() != 'TGNL') {
            same = same && mat->getTexGenBlock()->getTexMtxOffset() == rd16(p + 4);
        }
        if (mat->getTevBlock()->getType() != 'TVNL') {
            same = same && mat->getTevBlock()->getTexNoOffset() == rd16(p + 6);
        }
        if (mat->getPEBlock()->getType() == 'PEFL') {
            same = same && mat->getPEBlock()->getFogOffset() == rd16(p + 0xA);
        }
        if (!same) {
            fail("MDL3 material %u: patching offsets differ from the file's", i);
            return;
        }
    }
}

// The material table of a model or a BMT: count and names from MAT3/MAT2 (or MDL3 when the loader
// read only that), each material against the file.
void checkMaterials(J3DMaterialTable& table, const RawFile& raw, const LoadPlan& plan, int fd,
                    const String& path) {
    const RawBlock* mat3 = raw.find("MAT3");
    const RawBlock* mat2 = raw.find("MAT2");
    const RawBlock* mdl3 = raw.find("MDL3");
    const RawBlock* mat = mat3 != nullptr ? mat3 : mat2;
    // loadBinaryDisplayList reads MAT3 only with material type 0 or 0x2000 (J3DModelLoader.cpp).
    bool matRead = mat != nullptr && (plan.kind != kBdl || getBdlFlag_MaterialType(plan.flags) == 0 ||
                                      getBdlFlag_MaterialType(plan.flags) == 0x2000);
    const RawBlock* names = matRead ? mat : mdl3;
    if (names == nullptr) {
        // A BMT holding only TEX1 (a texture set for J3DModelData::setMaterialTable).
        if (table.getMaterialNum() != 0) {
            fail("%u materials, the file has no material block", table.getMaterialNum());
        }
        if (fd >= 0) {
            writef(fd, "MAT %s count=0 names=\n", path.c_str());
        }
        return;
    }
    uint16_t num = rd16(names->p + 8);
    if (table.getMaterialNum() != num) {
        fail("%u materials, the file has %u", table.getMaterialNum(), num);
        return;
    }
    uint32_t nameField = names == mdl3 ? 0x20 : 0x14;
    String joined = checkNames("material", table.getMaterialName(), raw.at(names, nameField), num,
                               names->p + names->size);
    if (matRead) {
        for (uint16_t i = 0; i < num; i++) {
            checkMaterial(table.getMaterialNodePointer(i), i, raw, mat, mat == mat3 ? kMat3 : kMat2,
                          plan.kind == kBdl);
        }
    }
    if (plan.kind == kBdl && mdl3 != nullptr) {
        checkMaterialDL(table, raw, mdl3, mat3, plan.flags);
    }
    if (mat2 != nullptr) {
        sTotals.v21++;
    }
    sTotals.materials += num;
    if (fd >= 0) {
        writeNamesLine(fd, "MAT", path, num, joined);
    }
}

void checkTextures(J3DMaterialTable& table, const RawFile& raw, int fd, const String& path) {
    const RawBlock* b = raw.find("TEX1");
    J3DTexture* tex = table.getTexture();
    if (b == nullptr) {
        if (tex != nullptr && tex->getNum() != 0) {
            fail("%u textures, the file has no TEX1", tex->getNum());
        }
        if (fd >= 0) {
            writef(fd, "TEX1 %s count=0\n", path.c_str());
        }
        return;
    }
    uint16_t num = rd16(b->p + 8);
    if (tex == nullptr || tex->getNum() != num ||
        (num != 0 && (const uint8_t*)tex->getResTIMG(0) != raw.at(b, 0x0C))) {
        fail("TEX1: %u textures at %p, the file has %u at %p", tex != nullptr ? tex->getNum() : 0,
             tex != nullptr && tex->getNum() != 0 ? (void*)tex->getResTIMG(0) : nullptr, num,
             (const void*)raw.at(b, 0x0C));
        return;
    }
    String names = checkNames("texture", table.getTextureName(), raw.at(b, 0x10), num,
                              b->p + b->size);
    sTotals.textures += num;
    if (fd >= 0) {
        writeNamesLine(fd, "TEX1", path, num, names);
    }
}

const char* kKindName[] = {"bmd", "bdl", "bmt"};

void sweepModel(const String& path, uint8_t* bytes, uint32_t size, const LoadPlan& plan,
                JKRHeap* parent, int fd) {
    sWhere = path.c_str();
    RawFile raw;
    if (!parseRaw(bytes, size, raw)) {
        fail("the file's header or blocks are malformed");
        return;
    }
    // A solid heap as dRes_info_c::setRes makes one, adjusted after the load.
    JKRSolidHeap* heap = mDoExt_createSolidHeapToCurrent(0, parent, 0x20);
    if (heap == nullptr) {
        fail("no solid heap (parent free %d)", (int)parent->getTotalFreeSize());
        return;
    }
    J3DModelData* md = nullptr;
    J3DMaterialTable* table = nullptr;
    switch (plan.kind) {
    case kBmd:
        md = J3DModelLoaderDataBase::load(bytes, plan.flags);
        break;
    case kBdl:
        md = J3DModelLoaderDataBase::loadBinaryDisplayList(bytes, plan.flags);
        break;
    case kBmt:
        table = J3DModelLoaderDataBase::loadMaterialTable(bytes);
        break;
    }
    mDoExt_restoreCurrentHeap();
    mDoExt_adjustSolidHeap(heap);
    if (md == nullptr && table == nullptr) {
        fail("the %s loader returned NULL", kKindName[plan.kind]);
        mDoExt_destroySolidHeap(heap);
        return;
    }
    sTotals.models[plan.kind]++;
    if (fd >= 0) {
        writef(fd, "J3D %s magic=%s loader=%s source=%s flags=0x%08x\n", path.c_str(), raw.magic,
               kKindName[plan.kind], plan.source, (unsigned)plan.flags);
    }
    if (md != nullptr) {
        if (md->getModelDataType() != (plan.kind == kBdl ? 1u : 0u)) {
            fail("model data type %u", (unsigned)md->getModelDataType());
        }
        checkInfo(md, raw, plan, fd, path);
        checkVertex(md, raw);
        checkEnvelope(md, raw, fd, path);
        checkDraw(md, raw, fd, path);
        checkJoints(md, raw, fd, path);
        checkShapes(md, raw, fd, path);
        table = &md->getMaterialTable();
    }
    checkMaterials(*table, raw, plan, fd, path);
    checkTextures(*table, raw, fd, path);
    mDoExt_destroySolidHeap(heap);
}

bool fileKind(const uint8_t* b, uint32_t size, LoaderKind& kind) {
    if (size < 8 || memcmp(b, "J3D2", 4) != 0) {
        return false;
    }
    if (memcmp(b + 4, "bmd2", 4) == 0 || memcmp(b + 4, "bmd3", 4) == 0) {
        kind = kBmd;
    } else if (memcmp(b + 4, "bdl4", 4) == 0) {
        kind = kBdl;
    } else if (memcmp(b + 4, "bmt3", 4) == 0) {
        kind = kBmt;
    } else {
        return false;
    }
    return true;
}

void walkArchive(JKRArchive* arc, uint32_t node, const String& arcPath, const String& prefix,
                 int depth, JKRHeap* heap, int fd) {
    if (depth > 16 || node >= arc->mArcInfoBlock->num_nodes) {
        fail("bad directory node %u", node);
        return;
    }
    JKRArchive::SDIDirEntry* n = arc->mNodes + node;
    uint32_t type = n->type;
    for (uint32_t k = n->first_file_index; k < n->first_file_index + n->num_entries; k++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const char* name = arc->mStringTable + e->getNameOffset();
        if (e->isDirectory()) {
            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                walkArchive(arc, e->data_offset, arcPath, prefix + name + "/", depth + 1, heap, fd);
            }
            continue;
        }
        String inner = prefix + name;
        uint8_t* bytes = (uint8_t*)arc->getResource(("/" + inner).c_str());
        if (bytes == nullptr) {
            continue;
        }
        uint32_t size = arc->getResSize(bytes);
        LoaderKind kind;
        if (!fileKind(bytes, size, kind)) {
            continue;
        }
        String path = arcPath + ":" + inner;
        sWhere = path.c_str();
        LoadPlan plan;
        if (!planFor(type, kind, plan)) {
            // dRes_info_c::loadResource would get NULL and fail the archive.
            fail("a %s file in a directory of type %.4s", kKindName[kind], (const char*)&n->type);
            continue;
        }
        if (strcmp(plan.source, "none") == 0 && kind == kBdl &&
            arcPath == "/res/Stage/sea/LODALL.arc") {
            plan.source = "lod"; // d_a_lod_bg: loadBinaryDisplayList(bin, 0x2020)
        }
        sweepModel(path, bytes, size, plan, heap, fd);
        sWhere = arcPath.c_str();
    }
}

void sweepArchive(const String& path, JKRHeap* heap, int fd) {
    sWhere = path.c_str();
    s32 heapFree = heap->getTotalFreeSize();
    JKRArchive* arc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, heap,
                                        JKRArchive::MOUNT_DIRECTION_HEAD);
    if (arc == nullptr) {
        fail("mount failed");
        return;
    }
    sTotals.archives++;
    walkArchive(arc, 0, path, "", 0, heap, fd);
    sWhere = path.c_str();
    arc->unmount();
    if (heap->getTotalFreeSize() != heapFree) {
        fail("after unmount the sweep heap lost %d bytes", (int)(heapFree - heap->getTotalFreeSize()));
    }
}

void findArchives(const String& dirPath, Vector<String>& out, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.c_str(), &dir)) {
        sWhere = dirPath.c_str();
        fail("DVDOpenDir failed");
        return;
    }
    DVDDirEntry entry;
    Vector<String> subdirs;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr) {
            continue;
        }
        String child = dirPath + "/" + entry.name;
        if (entry.isDir) {
            subdirs.push_back(child);
            continue;
        }
        size_t len = child.size();
        if (len > 4 && strcasecmp(child.c_str() + len - 4, ".arc") == 0) {
            out.push_back(child);
        }
    }
    DVDCloseDir(&dir);
    if (depth < 6) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

} // namespace

[[noreturn]] void smokeJ3dSweep() {
    uint64_t start = elapsedMs();
    sWhere = "j3d-sweep";
    Vector<String> archives;
    findArchives("/res", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] j3d-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("j3d_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# j3d-sweep (COS_SMOKE=j3d-sweep): every BMD/BDL/BMT as the game's J3D "
                   "loaders read it, in disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "j3d-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    unsigned int models = sTotals.models[0] + sTotals.models[1] + sTotals.models[2];
    writef(STDERR_FILENO,
           "[cos] j3d-sweep: %u archives, %u models (%u BMD, %u of them MAT2; %u BDL; %u BMT); "
           "%u joints, %u materials, %u shapes, %u textures, %u draw matrices, %u weighted "
           "matrices; %llu ms; %d error(s)%s\n",
           sTotals.archives, models, sTotals.models[0], sTotals.v21, sTotals.models[1],
           sTotals.models[2], sTotals.joints, sTotals.materials, sTotals.shapes, sTotals.textures,
           sTotals.drawMtx, sTotals.envelopes, (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in j3d_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && models > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
