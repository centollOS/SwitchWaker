// Collision (dzb) on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.10): the COS_SMOKE=dzb-sweep
// test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. Every .arc under
// /res/Stage and /res/Object (the FST walked with DVDOpenDir/DVDReadDir; the disc keeps its dzb
// files there) is mounted in main RAM, as dRes_info_c mounts it, and each file of its 'DZB '
// directory is fetched as dRes_info_c::loadResource fetches it (getFirstResource('DZB '),
// JKRGetTypeResource), then read twice:
// - independently: a copy of the file taken before the game touches it, read with plain
//   big-endian loads at the format's offsets (the 0x34-byte header, every vertex, triangle, block,
//   tree node, group with its name, and poly-info entry), no game struct;
// - through the game: cBgS::ConvDzb relocates and converts it in place, as loadResource does; the
//   cBgD_t header and every table entry read through the game's cBgD_* structs must equal the
//   copy (each table where the file's offset says, each group name at its offset, the vertices
//   bit for bit, the flag word with the converted bit 0x80000000 set); a second ConvDzb must leave
//   every byte unchanged.
// Then the collision is built as the game builds it: a dBgW in a solid heap, Set with
// cBgW::GLOBAL_e (as d_a_bg does for a room) and registered in a dBgS of this test. Downward
// dBgS_GndChk rays go through dBgS::GroundCross:
// - a 9 x 9 grid over the vertices' x/z bounding box, from above its top: every hit must lie
//   inside the bounding box (in y, widened only by the rise of the reported triangle's plane over
//   the edge margin below, as the game allows a ray just outside a sloped triangle to stop on
//   it), and the triangle it reports (read from the copy) must contain the
//   ray's x/z (with the 20-unit edge margin of cM3d_CrossY_Tri_Front) and give the hit height
//   at that point;
// - one ray at the centroid of up to 48 ground triangles (normal y >= 0.55, not degenerate, under
//   the tree of a group the ray can reach: the root group's tree, less what the default
//   dBgS_GrpPassChk skips): the hit must be at least as high as that triangle at that point.
// The same dzb is then set a second time with cBgW::MOVE_BG_e and an identity base matrix (the
// path movable actors use: the vertices copied through GlobalVtx), and the grid must give the
// same heights and triangles.
// <COS_RUN_DIR>/dzb_sweep.txt gets a DZB line per file ('<archive>:<directory>/<name>') (counts, offsets, flag, vertex bounding
// box as the game read them, in disc_manifest.py's names); native/tools/run.sh compares it
// with the manifest (disc_manifest.py --check-dzb).
// The test's own strings and vectors are in host memory (cos_sdk/host_alloc.h).
// Exit 0 when every check holds, 1 otherwise.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRFileFinder.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "SSystem/SComponent/c_bg_s.h"
#include "SSystem/SComponent/c_bg_w.h"
#include "d/d_bg_s.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_w.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"

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

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr uint32_t kSweepHeapSize = 24 * 1024 * 1024;
constexpr int kGrid = 9;
constexpr int kMaxGroundRays = 48;
constexpr uint16_t kNone = 0xFFFF;

int sErrors = 0;
const char* sWhere = "";

struct Totals {
    uint32_t archives = 0, files = 0, vertices = 0, triangles = 0, groups = 0;
    uint32_t rays = 0, hits = 0, groundRays = 0;
} sTotals;

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] dzb-sweep: %s: %s\n", sWhere, text);
    }
    sErrors++;
}

uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

float rdf(const uint8_t* p) {
    uint32_t bits = rd32(p);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

uint32_t floatBits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, 4);
    return bits;
}

// ---- the independent reading -------------------------------------------------------------------

// Table entry sizes of the format.
constexpr uint32_t kVtx = 0xC, kTri = 0xA, kBlk = 0x2, kTree = 0x14, kGrp = 0x34, kTi = 0x10;

struct RawDzb {
    Vector<uint8_t> bytes; // the file before ConvDzb
    int32_t num[6];        // vertices, triangles, blocks, tree nodes, groups, infos
    uint32_t off[6];       // their offsets
    uint32_t flag;
    const uint8_t* at(int table, int i) const {
        static const uint32_t kSize[6] = {kVtx, kTri, kBlk, kTree, kGrp, kTi};
        return bytes.data() + off[table] + (uint32_t)i * kSize[table];
    }
    double vx(int i, int c) const { return rdf(at(0, i) + c * 4); }
    bool hasVertices() const { return off[0] != 0 && num[0] > 0; }
};

enum { kV, kT, kB, kN, kG, kI };

bool parseRaw(const uint8_t* b, uint32_t size, RawDzb& raw) {
    raw.bytes.assign(b, b + size);
    if (size < 0x34) {
        fail("%u bytes, shorter than the header", size);
        return false;
    }
    static const uint32_t kSize[6] = {kVtx, kTri, kBlk, kTree, kGrp, kTi};
    for (int t = 0; t < 6; t++) {
        raw.num[t] = (int32_t)rd32(b + t * 8);
        raw.off[t] = rd32(b + t * 8 + 4);
        // A vertex offset of 0 is "no vertex table" (ConvDzb leaves it null; one dzb on the disc,
        // a deforming sand floor whose owner supplies the vertices).
        if (t == kV && raw.off[t] == 0) {
            continue;
        }
        if (raw.num[t] < 0 || (uint64_t)raw.off[t] + (uint64_t)raw.num[t] * kSize[t] > size) {
            fail("table %d: %d entries at 0x%x past the end (%u bytes)", t, raw.num[t],
                 raw.off[t], size);
            return false;
        }
    }
    raw.flag = rd32(b + 0x30);
    if (raw.flag & 0x80000000) {
        fail("flag 0x%08x: already converted on the disc", raw.flag);
        return false;
    }
    for (int g = 0; g < raw.num[kG]; g++) {
        uint32_t name = rd32(raw.at(kG, g));
        if (name == 0 || name >= size || memchr(b + name, 0, size - name) == nullptr) {
            fail("group %d: name offset 0x%x outside the file", g, name);
            return false;
        }
    }
    return true;
}

// ---- the game's reading against it -------------------------------------------------------------

template <class T>
bool tableAt(const char* what, T* ptr, const RawDzb& raw, int t, const uint8_t* base) {
    const uint8_t* want = raw.off[t] != 0 ? base + raw.off[t] : nullptr;
    if ((const uint8_t*)ptr != want) {
        fail("%s table at %p; the file has offset 0x%x (%p)", what, (const void*)ptr, raw.off[t],
             (const void*)want);
        return false;
    }
    return true;
}

void checkConverted(cBgD_t* bgd, const RawDzb& raw) {
    const uint8_t* base = (const uint8_t*)bgd;
    int32_t gotNum[6] = {bgd->m_v_num, bgd->m_t_num, bgd->m_b_num, bgd->m_tree_num, bgd->m_g_num,
                         bgd->m_ti_num};
    for (int t = 0; t < 6; t++) {
        if (gotNum[t] != raw.num[t]) {
            fail("table %d: %d entries; the file has %d", t, gotNum[t], raw.num[t]);
        }
    }
    if ((uint32_t)bgd->flag != (raw.flag | 0x80000000)) {
        fail("flag 0x%08x after ConvDzb; the file has 0x%08x", (uint32_t)bgd->flag, raw.flag);
    }
    bool ok = tableAt("vertex", (cBgD_Vtx_t*)bgd->m_v_tbl, raw, kV, base) &
              tableAt("triangle", (cBgD_Tri_t*)bgd->m_t_tbl, raw, kT, base) &
              tableAt("block", (cBgD_Blk_t*)bgd->m_b_tbl, raw, kB, base) &
              tableAt("tree", (cBgD_Tree_t*)bgd->m_tree_tbl, raw, kN, base) &
              tableAt("group", (cBgD_Grp_t*)bgd->m_g_tbl, raw, kG, base) &
              tableAt("info", (cBgD_Ti_t*)bgd->m_ti_tbl, raw, kI, base);
    if (!ok) {
        return;
    }

    const cBgD_Vtx_t* vtx = bgd->m_v_tbl;
    for (int i = 0; raw.hasVertices() && i < raw.num[kV]; i++) {
        const uint8_t* r = raw.at(kV, i);
        if (floatBits(vtx[i].x) != rd32(r) || floatBits(vtx[i].y) != rd32(r + 4) ||
            floatBits(vtx[i].z) != rd32(r + 8)) {
            fail("vertex %d: (%g, %g, %g); the file has (%g, %g, %g)", i, vtx[i].x, vtx[i].y,
                 vtx[i].z, rdf(r), rdf(r + 4), rdf(r + 8));
            break;
        }
    }
    const cBgD_Tri_t* tri = bgd->m_t_tbl;
    for (int i = 0; i < raw.num[kT]; i++) {
        const uint8_t* r = raw.at(kT, i);
        u16 got[5] = {tri[i].vtx0, tri[i].vtx1, tri[i].vtx2, tri[i].id, tri[i].grp};
        for (int k = 0; k < 5; k++) {
            if (got[k] != rd16(r + k * 2)) {
                fail("triangle %d field %d: %u; the file has %u", i, k, got[k], rd16(r + k * 2));
                i = raw.num[kT];
                break;
            }
        }
    }
    const cBgD_Blk_t* blk = bgd->m_b_tbl;
    for (int i = 0; i < raw.num[kB]; i++) {
        if (blk[i].startTri != rd16(raw.at(kB, i))) {
            fail("block %d: first triangle %u; the file has %u", i, (u16)blk[i].startTri,
                 rd16(raw.at(kB, i)));
            break;
        }
    }
    const cBgD_Tree_t* tree = bgd->m_tree_tbl;
    for (int i = 0; i < raw.num[kN]; i++) {
        const uint8_t* r = raw.at(kN, i);
        bool same = tree[i].mFlag == rd16(r) && tree[i].mParent == rd16(r + 2) &&
                    tree[i].mBlock == rd16(r + 4);
        for (int k = 0; k < 8; k++) {
            same = same && tree[i].mChild[k] == rd16(r + 4 + k * 2);
        }
        if (!same) {
            fail("tree node %d differs from the file (flag %u, parent %u)", i, (u16)tree[i].mFlag,
                 (u16)tree[i].mParent);
            break;
        }
    }
    const cBgD_Grp_t* grp = bgd->m_g_tbl;
    for (int i = 0; i < raw.num[kG]; i++) {
        const uint8_t* r = raw.at(kG, i);
        const char* name = grp[i].m_name;
        cXyz scale = grp[i].m_scale, trans = grp[i].m_translation;
        csXyz rot = grp[i].m_rotation;
        bool same = (const uint8_t*)name == base + rd32(r) &&
                    floatBits(scale.x) == rd32(r + 0x04) && floatBits(scale.y) == rd32(r + 0x08) &&
                    floatBits(scale.z) == rd32(r + 0x0C) && (u16)rot.x == rd16(r + 0x10) &&
                    (u16)rot.y == rd16(r + 0x12) && (u16)rot.z == rd16(r + 0x14) &&
                    floatBits(trans.x) == rd32(r + 0x18) && floatBits(trans.y) == rd32(r + 0x1C) &&
                    floatBits(trans.z) == rd32(r + 0x20) && grp[i].m_parent == rd16(r + 0x24) &&
                    grp[i].m_next_sibling == rd16(r + 0x26) &&
                    grp[i].m_first_child == rd16(r + 0x28) && grp[i].m_room_id == rd16(r + 0x2A) &&
                    grp[i].m_first_vtx_idx == rd16(r + 0x2C) &&
                    grp[i].m_tree_idx == rd16(r + 0x2E) && grp[i].m_info == rd32(r + 0x30);
        if (!same) {
            fail("group %d (%p, name offset 0x%x) differs from the file", i, (const void*)name,
                 rd32(r));
            break;
        }
    }
    const cBgD_Ti_t* ti = bgd->m_ti_tbl;
    for (int i = 0; i < raw.num[kI]; i++) {
        const uint8_t* r = raw.at(kI, i);
        if (ti[i].mPolyInf0 != rd32(r) || ti[i].mPolyInf1 != rd32(r + 4) ||
            ti[i].mPolyInf2 != rd32(r + 8) || ti[i].mPolyInf3 != rd32(r + 12)) {
            fail("poly info %d: %08x %08x %08x %08x; the file has %08x %08x %08x %08x", i,
                 (u32)ti[i].mPolyInf0, (u32)ti[i].mPolyInf1, (u32)ti[i].mPolyInf2,
                 (u32)ti[i].mPolyInf3, rd32(r), rd32(r + 4), rd32(r + 8), rd32(r + 12));
            break;
        }
    }
}

// ---- rays ------------------------------------------------------------------------------------

struct Box {
    double min[3] = {0, 0, 0}, max[3] = {0, 0, 0};
    double extent() const {
        double e = 0;
        for (int c = 0; c < 3; c++) {
            e = fmax(e, fmax(fabs(min[c]), fabs(max[c])));
        }
        return e;
    }
};

struct Tri {
    double p[3][3];
    double n[3]; // unit normal
    double area; // in x/z
};

Tri rawTri(const RawDzb& raw, int t) {
    Tri tr;
    const uint8_t* r = raw.at(kT, t);
    for (int k = 0; k < 3; k++) {
        int v = rd16(r + k * 2);
        for (int c = 0; c < 3; c++) {
            tr.p[k][c] = raw.vx(v, c);
        }
    }
    double a[3], b[3];
    for (int c = 0; c < 3; c++) {
        a[c] = tr.p[1][c] - tr.p[0][c];
        b[c] = tr.p[2][c] - tr.p[0][c];
    }
    double n[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    double len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (int c = 0; c < 3; c++) {
        tr.n[c] = len > 0 ? n[c] / len : 0;
    }
    tr.area = fabs(n[1]) / 2;
    return tr;
}

// Distance in x/z from (x, z) to the triangle's projection (0 inside).
double xzDistance(const Tri& tr, double x, double z) {
    double d = 0;
    bool inside = true;
    double sign = 0;
    for (int k = 0; k < 3; k++) {
        const double* a = tr.p[k];
        const double* b = tr.p[(k + 1) % 3];
        double ex = b[0] - a[0], ez = b[2] - a[2];
        double cross = ex * (z - a[2]) - ez * (x - a[0]);
        if (cross != 0) {
            if (sign == 0) {
                sign = cross;
            } else if ((cross > 0) != (sign > 0)) {
                inside = false;
            }
        }
    }
    if (inside) {
        return 0;
    }
    d = 1e30;
    for (int k = 0; k < 3; k++) {
        const double* a = tr.p[k];
        const double* b = tr.p[(k + 1) % 3];
        double ex = b[0] - a[0], ez = b[2] - a[2];
        double len2 = ex * ex + ez * ez;
        double t = len2 > 0 ? ((x - a[0]) * ex + (z - a[2]) * ez) / len2 : 0;
        t = fmin(1, fmax(0, t));
        double dx = a[0] + t * ex - x, dz = a[2] + t * ez - z;
        d = fmin(d, sqrt(dx * dx + dz * dz));
    }
    return d;
}

// The triangle's height at (x, z) (its plane).
double planeY(const Tri& tr, double x, double z) {
    const double* a = tr.p[0];
    return a[1] - (tr.n[0] * (x - a[0]) + tr.n[2] * (z - a[2])) / tr.n[1];
}

struct Hit {
    float y;
    int poly;
};

Hit groundCross(dBgS& bgs, double x, double y, double z) {
    dBgS_GndChk chk;
    cXyz pos((f32)x, (f32)y, (f32)z);
    chk.SetPos(&pos);
    f32 got = bgs.GroundCross(&chk);
    return Hit{got, got > -G_CM3D_F_INF ? chk.GetPolyIndex() : -1};
}

// The groups a default dBgS_GndChk reaches, from the copy: cBgW::GroundCross starts at the first
// group with no parent (m_rootGrpIdx) and walks first-child/next-sibling links
// (cBgW::GroundCrossGrpRp); the default dBgS_GrpPassChk skips a group at depth 2 whose info has a
// special-group bit, and everything under it (dBgW::ChkGrpThrough). A group outside the root's
// tree is never reached (the disc has such files, e.g. several parentless groups).
Vector<bool> reachedGroups(const RawDzb& raw) {
    int n = raw.num[kG];
    Vector<bool> reached(n, false);
    auto field = [&](int g, uint32_t off) { return rd16(raw.at(kG, g) + off); };
    int root = -1;
    for (int g = 0; g < n && root < 0; g++) {
        if (field(g, 0x24) == kNone) {
            root = g;
        }
    }
    if (root < 0) {
        return reached;
    }
    struct Item {
        int g, depth;
    };
    Vector<Item> stack;
    stack.push_back({root, 1});
    while (!stack.empty()) {
        Item it = stack.back();
        stack.pop_back();
        if (it.g >= n || reached[it.g]) {
            continue;
        }
        if (it.depth == 2 && (rd32(raw.at(kG, it.g) + 0x30) & 0x80700) != 0) {
            continue;
        }
        reached[it.g] = true;
        for (int c = field(it.g, 0x28); c != kNone && c < n; c = field(c, 0x26)) {
            stack.push_back({c, it.depth + 1});
            if (stack.size() > (size_t)n * 4) {
                break;
            }
        }
    }
    return reached;
}

// The triangles a default dBgS_GndChk can reach: those of the blocks under the tree node of each
// reached group (cBgW::GroundCrossRp: a leaf node, flag bit 0, holds a block; a branch node its
// eight children), from the copy.
Vector<bool> reachedTriangles(const RawDzb& raw) {
    Vector<bool> groups = reachedGroups(raw);
    Vector<bool> tris(raw.num[kT], false);
    Vector<bool> seen(raw.num[kN], false);
    Vector<int> stack;
    for (int g = 0; g < raw.num[kG]; g++) {
        int node = rd16(raw.at(kG, g) + 0x2E);
        if (groups[g] && node != kNone) {
            stack.push_back(node);
        }
    }
    while (!stack.empty()) {
        int node = stack.back();
        stack.pop_back();
        if (node >= raw.num[kN] || seen[node]) {
            continue;
        }
        seen[node] = true;
        const uint8_t* r = raw.at(kN, node);
        if (rd16(r) & 1) {
            int blk = rd16(r + 4);
            if (blk >= raw.num[kB]) {
                continue;
            }
            int first = rd16(raw.at(kB, blk));
            int last = blk + 1 < raw.num[kB] ? rd16(raw.at(kB, blk + 1)) - 1 : raw.num[kT] - 1;
            for (int t = first; t <= last && t < raw.num[kT]; t++) {
                tris[t] = true;
            }
        } else {
            for (int k = 0; k < 8; k++) {
                int child = rd16(r + 4 + k * 2);
                if (child != kNone) {
                    stack.push_back(child);
                }
            }
        }
    }
    return tris;
}

// The game's x/z inclusion test (cM3d_CrossY_Tri_Front): inside the vertices' x/z box and on the
// inner side of each edge with a margin of 20 in the edge's cross product.
bool insideXz(const Tri& tr, double x, double z) {
    double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
    for (int k = 0; k < 3; k++) {
        lo[0] = fmin(lo[0], tr.p[k][0]);
        hi[0] = fmax(hi[0], tr.p[k][0]);
        lo[1] = fmin(lo[1], tr.p[k][2]);
        hi[1] = fmax(hi[1], tr.p[k][2]);
    }
    double eps = 1e-3 + 1e-5 * fmax(fabs(x), fabs(z));
    if (x < lo[0] - eps || x > hi[0] + eps || z < lo[1] - eps || z > hi[1] + eps) {
        return false;
    }
    for (int k = 0; k < 3; k++) {
        const double* a = tr.p[k];
        const double* b = tr.p[(k + 1) % 3];
        // cM3d_VectorProduct2d(a.z, a.x, b.z, b.x, z, x)
        double cross = (b[2] - a[2]) * (x - a[0]) - (b[0] - a[0]) * (z - a[2]);
        double scale = fabs((b[2] - a[2]) * (x - a[0])) + fabs((b[0] - a[0]) * (z - a[2]));
        if (cross < -20.0 - 1e-5 * scale - 1e-3) {
            return false;
        }
    }
    return true;
}

void castRays(cBgD_t* bgd, const RawDzb& raw, const Box& box, JKRHeap* sweepHeap) {
    if (!raw.hasVertices() || raw.num[kT] == 0) {
        return;
    }
    double tol = 0.05 + 2e-5 * box.extent();
    double top = box.max[1] + 100.0;

    JKRSolidHeap* heap = JKRSolidHeap::create((u32)-1, sweepHeap, false);
    if (heap == nullptr) {
        fail("no solid heap (sweep heap free %d)", (int)sweepHeap->getTotalFreeSize());
        return;
    }
    JKRHeap* prev = heap->becomeCurrentHeap();
    static dBgS sBgS;
    sBgS.Ct();

    // The room path: the vertex table used in place, global coordinates.
    dBgW* global = new dBgW();
    if (global == nullptr || global->Set(bgd, cBgW::GLOBAL_e, NULL) ||
        sBgS.cBgS::Regist(global, fpcM_ERROR_PROCESS_ID_e, NULL)) {
        fail("dBgW::Set (GLOBAL_e) or Regist failed");
        prev->becomeCurrentHeap();
        mDoExt_destroySolidHeap(heap);
        return;
    }
    if (global->GetVtxTbl() != (cBgD_Vtx_t*)bgd->m_v_tbl) {
        fail("GLOBAL_e vertex table %p, not the file's %p", (void*)global->GetVtxTbl(),
             (void*)(cBgD_Vtx_t*)bgd->m_v_tbl);
    }

    Hit grid[kGrid][kGrid];
    double gx[kGrid], gz[kGrid];
    for (int i = 0; i < kGrid; i++) {
        double f = (i + 0.5) / kGrid;
        gx[i] = box.min[0] + f * (box.max[0] - box.min[0]);
        gz[i] = box.min[2] + f * (box.max[2] - box.min[2]);
    }
    for (int i = 0; i < kGrid; i++) {
        for (int k = 0; k < kGrid; k++) {
            Hit h = groundCross(sBgS, gx[i], top, gz[k]);
            grid[i][k] = h;
            sTotals.rays++;
            if (h.poly < 0) {
                continue;
            }
            sTotals.hits++;
            if (h.poly >= raw.num[kT]) {
                fail("ray (%g, %g): hit y %g on triangle %d of %d", gx[i], gz[k], h.y, h.poly,
                     raw.num[kT]);
                continue;
            }
            Tri tr = rawTri(raw, h.poly);
            bool inside = insideXz(tr, gx[i], gz[k]);
            double d = xzDistance(tr, gx[i], gz[k]);
            double py = fabs(tr.n[1]) > 1e-6 ? planeY(tr, gx[i], gz[k]) : h.y;
            if (!inside || fabs(py - h.y) > tol + 1e-4 * fabs(h.y)) {
                fail("ray (%g, %g): hit y %g on triangle %d, which is %g away in x/z (%s) and at "
                     "y %g there", gx[i], gz[k], h.y, h.poly, d, inside ? "inside" : "outside",
                     py);
                continue;
            }
            // The edge margin lets a ray just outside a sloped triangle stop on its plane: the
            // box is widened by that rise (distance times slope) and no more.
            double rise = fabs(tr.n[1]) > 1e-6
                              ? d * sqrt(tr.n[0] * tr.n[0] + tr.n[2] * tr.n[2]) / fabs(tr.n[1])
                              : 0;
            if (!(h.y >= box.min[1] - tol - rise && h.y <= box.max[1] + tol + rise)) {
                fail("ray (%g, %g): hit y %g on triangle %d outside the box y %g..%g (margin %g)",
                     gx[i], gz[k], h.y, h.poly, box.min[1], box.max[1], rise);
            }
        }
    }

    // Ground triangles: a ray at each one's centroid must stop on it or above it.
    Vector<bool> reached = reachedTriangles(raw);
    Vector<int> ground;
    for (int t = 0; t < raw.num[kT]; t++) {
        Tri tr = rawTri(raw, t);
        if (tr.n[1] >= 0.55 && tr.area > 1.0 && reached[t]) {
            ground.push_back(t);
        }
    }
    size_t step = ground.size() > (size_t)kMaxGroundRays ? ground.size() / kMaxGroundRays : 1;
    for (size_t j = 0; j < ground.size(); j += step) {
        Tri tr = rawTri(raw, ground[j]);
        double cx = (tr.p[0][0] + tr.p[1][0] + tr.p[2][0]) / 3;
        double cy = (tr.p[0][1] + tr.p[1][1] + tr.p[2][1]) / 3;
        double cz = (tr.p[0][2] + tr.p[1][2] + tr.p[2][2]) / 3;
        Hit h = groundCross(sBgS, cx, top, cz);
        sTotals.groundRays++;
        if (h.poly < 0 || h.y < cy - tol) {
            fail("ground triangle %d: ray at (%g, %g) hit y %g (triangle %d); the triangle is at "
                 "y %g", ground[j], cx, cz, h.y, h.poly, cy);
        }
    }
    sBgS.Release(global);

    // The movable path: vertices copied through the base matrix (identity here).
    static Mtx sIdentity;
    MTXIdentity(sIdentity);
    dBgW* moving = new dBgW();
    if (moving == nullptr || moving->Set(bgd, cBgW::MOVE_BG_e, &sIdentity) ||
        sBgS.cBgS::Regist(moving, fpcM_ERROR_PROCESS_ID_e, NULL)) {
        fail("dBgW::Set (MOVE_BG_e) or Regist failed");
    } else {
        for (int i = 0; i < kGrid; i++) {
            for (int k = 0; k < kGrid; k++) {
                Hit h = groundCross(sBgS, gx[i], top, gz[k]);
                if (h.poly != grid[i][k].poly || floatBits(h.y) != floatBits(grid[i][k].y)) {
                    fail("ray (%g, %g): MOVE_BG_e hit y %g triangle %d; GLOBAL_e %g triangle %d",
                         gx[i], gz[k], h.y, h.poly, grid[i][k].y, grid[i][k].poly);
                    i = kGrid;
                    break;
                }
            }
        }
        sBgS.Release(moving);
    }
    prev->becomeCurrentHeap();
    mDoExt_destroySolidHeap(heap);
}

void sweepDzb(const String& name, uint8_t* data, uint32_t size, JKRHeap* heap, int fd) {
    RawDzb raw;
    if (!parseRaw(data, size, raw)) {
        return;
    }
    // dRes_info_c::loadResource's conversion.
    cBgD_t* bgd = (cBgD_t*)cBgS::ConvDzb(data);
    if ((uint8_t*)bgd != data) {
        fail("ConvDzb returned %p for %p", (void*)bgd, (void*)data);
        return;
    }
    checkConverted(bgd, raw);
    // A second conversion must leave the file as it is.
    Vector<uint8_t> once(data, data + size);
    cBgS::ConvDzb(data);
    if (memcmp(once.data(), data, size) != 0) {
        fail("a second ConvDzb changed the file");
    }

    Box box;
    const cBgD_Vtx_t* vtx = bgd->m_v_tbl;
    for (int i = 0; raw.hasVertices() && i < raw.num[kV]; i++) {
        double v[3] = {vtx[i].x, vtx[i].y, vtx[i].z};
        for (int c = 0; c < 3; c++) {
            if (!std::isfinite(v[c])) {
                fail("vertex %d is not finite", i);
                return;
            }
            box.min[c] = i == 0 ? v[c] : fmin(box.min[c], v[c]);
            box.max[c] = i == 0 ? v[c] : fmax(box.max[c], v[c]);
        }
    }
    if (fd >= 0) {
        writef(fd,
               "DZB %s vertices=%d triangles=%d blocks=%d tree_nodes=%d groups=%d infos=%d "
               "flag=%u offsets=%u,%u,%u,%u,%u,%u",
               name.c_str(), (int)bgd->m_v_num, (int)bgd->m_t_num, (int)bgd->m_b_num,
               (int)bgd->m_tree_num, (int)bgd->m_g_num, (int)bgd->m_ti_num,
               (uint32_t)bgd->flag & 0x7FFFFFFF,
               vtx != nullptr ? (uint32_t)((const uint8_t*)vtx - data) : 0u,
               (uint32_t)((uint8_t*)(cBgD_Tri_t*)bgd->m_t_tbl - data),
               (uint32_t)((uint8_t*)(cBgD_Blk_t*)bgd->m_b_tbl - data),
               (uint32_t)((uint8_t*)(cBgD_Tree_t*)bgd->m_tree_tbl - data),
               (uint32_t)((uint8_t*)(cBgD_Grp_t*)bgd->m_g_tbl - data),
               (uint32_t)((uint8_t*)(cBgD_Ti_t*)bgd->m_ti_tbl - data));
        if (raw.hasVertices()) {
            writef(fd, " bbox=%.9g,%.9g,%.9g,%.9g,%.9g,%.9g", box.min[0], box.min[1], box.min[2],
                   box.max[0], box.max[1], box.max[2]);
        }
        writef(fd, "\n");
    }

    castRays(bgd, raw, box, heap);
    sTotals.files++;
    sTotals.vertices += raw.num[kV];
    sTotals.triangles += raw.num[kT];
    sTotals.groups += raw.num[kG];
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
    // dRes_info_c::loadResource's lookup for 'DZB '. The report names a file by its directory
    // (the manifest's '<archive>:dzb/<name>'): a dzb elsewhere in an archive (one stage archive
    // has a copy under dzs/) is not collision the game converts.
    Vector<String> names;
    String dirName;
    if (JKRArchive::SDIDirEntry* dir = arc->findResType('DZB ')) {
        dirName = String(arc->mStringTable + dir->name_offset) + "/";
    }
    JKRHeap* prev = heap->becomeCurrentHeap();
    JKRFileFinder* finder = arc->getFirstResource('DZB ');
    for (; JKRIsFileFinderAvailable(finder); finder->findNextFile()) {
        if (finder->isDirectory() || strcmp(finder->mEntryName, ".") == 0 ||
            strcmp(finder->mEntryName, "..") == 0) {
            continue;
        }
        names.push_back(finder->mEntryName);
    }
    delete finder;
    prev->becomeCurrentHeap();
    for (const String& entry : names) {
        void* res = JKRGetTypeResource('DZB ', entry.c_str(), arc);
        String name = path + ":" + dirName + entry;
        sWhere = name.c_str();
        if (res == nullptr) {
            fail("JKRGetTypeResource failed");
        } else {
            sweepDzb(name, (uint8_t*)res, arc->getResSize(res), heap, fd);
        }
        sWhere = path.c_str();
    }
    arc->unmount();
    if (heap->getTotalFreeSize() != heapFree) {
        fail("after unmount the sweep heap lost %d bytes",
             (int)(heapFree - heap->getTotalFreeSize()));
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
    if (depth < 4) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

} // namespace

[[noreturn]] void smokeDzbSweep() {
    uint64_t start = elapsedMs();
    sWhere = "dzb-sweep";
    Vector<String> archives;
    findArchives("/res/Stage", archives, 0);
    findArchives("/res/Object", archives, 0);
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRExpHeap* heap = JKRExpHeap::create(kSweepHeapSize, root, false);
    if (heap == nullptr) {
        writef(STDERR_FILENO, "[cos] dzb-sweep: no test heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    int fd = openRunFile("dzb_sweep.txt");
    if (fd >= 0) {
        writef(fd, "# dzb-sweep (COS_SMOKE=dzb-sweep): every dzb as the game read it, in "
                   "disc_manifest.py's names\n");
    }
    for (const String& path : archives) {
        sweepArchive(path, heap, fd);
        pc_frame_tick();
    }
    if (fd >= 0) {
        close(fd);
    }
    sWhere = "dzb-sweep";
    if (!heap->check()) {
        fail("JKRExpHeap::check failed on the sweep heap");
    }
    writef(STDERR_FILENO,
           "[cos] dzb-sweep: %u archives, %u dzb files, %u vertices, %u triangles, %u groups; "
           "%u grid rays (%u hits), %u ground-triangle rays; %llu ms; %d error(s)%s\n",
           sTotals.archives, sTotals.files, sTotals.vertices, sTotals.triangles, sTotals.groups,
           sTotals.rays, sTotals.hits, sTotals.groundRays,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (report in dzb_sweep.txt)" : "");
    bool pass = sErrors == 0 && sTotals.archives == archives.size() && sTotals.files > 0;
    pc_exit(pass ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
