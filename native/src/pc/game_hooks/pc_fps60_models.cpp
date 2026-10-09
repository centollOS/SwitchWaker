// COS_FPS60_TEST step C (docs/FPS60_PLAN.md, "Step C"): paint B shows the J3D models at t between where
// the draw pass before put them (the paint A just shown) and where this frame's draw pass put them, so
// what the camera follows (Link, the boat) no longer shakes against the blended camera of step B.
//
// - Capture: J3DModel::viewCalc reports itself (pc_fps60_model_viewcalc) while the draw pass runs
//   (fpcM_Management's pc_fps60_models_draw_begin/_end around fpcDw_Handler; calls from the painter,
//   such as dDlst_shadowReal_c::imageDraw's, and outside the draw pass are ignored) with the view it
//   used (j3dSys's: the camera's, or a shadow's light view in dDlst_shadowReal_c::set). At the draw
//   pass's end the listed models' matrices are copied (this frame's "cur"; the last "cur" becomes
//   "prev"), by how the model data's shapes load them (J3DModelData flags & 0xF0):
//     0x20 (J3DModelData.h's "NoUseDrawMtx", J3DShapeFactory's ConcatView; nearly every model of the
//          game): the world-space joint and envelope matrices (mpNodeMtx, mpWeightEnvMtx), which the
//          shapes concatenate with the view at paint time (J3DShapeMtxConcatView, immediate loads);
//          in calc mode 2 also mViewBaseMtx (view * base);
//     0x00 / 0x10: the view-space draw matrices viewCalc made (mpDrawMtxBuf[1][view]) and, for 0x00,
//          the normal matrices (indexed loads / immediate loads);
//     CPU skinning (J3DSkinDeform: the boat's body, Link's mirror shield): the root joint's matrix.
//   Host memory only; the game's memory is not written.
// - Paint B (pc_fps60_models_paint_begin, from pc_fps60_view_begin unless the camera is cut): for each
//   model in both captures (same J3DModel, not re-made at that address since, same model data, kind
//   and matrix count, view-space ones made with the two cameras' views, the matrices unchanged since
//   the capture):
//     world:      W_t = blend(W_prev, W_cur)
//     view space: D_t = blend(K * D_prev, D_cur) = V_cur * blend(W_prev, W_cur), K = V_cur * V_prev^-1
//     CPU skin:   the packets' base (position) matrix V_cur * R_t * R_cur^-1 (R: the root joint): the
//                 skinned vertices move rigidly with the root; their deformation stays N+1's
//   blend = the 3x3 part per column, rescaled to the lerped length (a slerp-like turn keeping the
//   scale), the translation linearly; view-space billboard joints keep D_cur's rotation (they face the
//   camera); normal matrices alike (K's rotation). Aurora's view delta (step B) then turns V_cur into
//   V_t. A model one of whose matrices moved more than 300 units, or whose first (root) matrix turned
//   more than 45 degrees (another one more than 120: a fast limb is blended), is drawn as captured.
//   The results go to a scratch arena; the model's pointers (mpNodeMtx/mpWeightEnvMtx,
//   mpDrawMtxBuf[1][view]/mpNrmMtxBuf[1][view], or each shape packet's base matrix pointer) point at
//   it, mViewBaseMtx is overwritten, until pc_fps60_models_paint_end (pc_fps60_view_end, after the
//   3D) puts the model's own back. Shapes read them when they draw on the game thread; an indexed load
//   goes through GXSetArray, which records the address in the GX stream, so putting the pointer back
//   afterwards does not change what was recorded.
// - pc_fps60_packet_mtx does the view-space blend for custom packets that load a matrix the draw pass
//   made (the boat's sail, daGrid_c, and the pirate ship's, daSail_packet_c): paint A's matrix (the
//   draw pass before's) is "prev".
// - The arena's lifetime: Aurora's GX worker reads indexed matrix loads (GX_LOAD_INDX_A/B) from the
//   recorded address when it translates the commands, possibly after paint B has returned (patch 0018,
//   async end of frame). The arena of a paint B is not written again until two more paints B have
//   used the other two arenas; by then the frame of the first has been processed: the split present
//   after it (aurora_end_frame waits while more than one ended frame is not done), the next draw
//   pass's GXDrawDone (a full drain) and, synchronously, every aurora_end_frame's drain all come in
//   between. The arenas are never freed (GXSetArray state may keep pointing into them).
// - Not covered (drawn as the draw pass left them, at N+1): static models (no animation: no
//   matrices of their own), CPU skinning in calc mode 2, models viewCalc'd only outside the draw pass,
//   the shadow image of real shadows (dDlst_shadowReal_c, cast in the painter from N+1's
//   matrices), non-J3D geometry (JPA particles, the ship's wake and waves, other custom packets: step
//   D). COS_FPS60_MODELS_LOG=1 logs the per-reason counts every 2 s; costs in the perf line.
//
// COS_FPS60_MODELS=0 turns it off (step B alone).
#include "JSystem/JSystem.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "pc_internal.h"

#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "dolphin/mtx/mtx.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <unistd.h>

struct PcFps60ModelAccess {
    static J3DModelData* data(J3DModel* m) { return m->mModelData; }
    static u32 flags(J3DModel* m) { return m->mFlags; }
    static u32 viewNo(J3DModel* m) { return m->mCurrentViewNo; }
    static Mtx**& drawArr(J3DModel* m) { return m->mpDrawMtxBuf[1]; }
    static Mtx33**& nrmArr(J3DModel* m) { return m->mpNrmMtxBuf[1]; }
    static Mtx*& nodes(J3DModel* m) { return m->mpNodeMtx; }
    static Mtx*& weights(J3DModel* m) { return m->mpWeightEnvMtx; }
    static MtxP viewBase(J3DModel* m) { return m->mViewBaseMtx; }
    static bool noUseDrawMtx(J3DModel* m) { return m->mpDrawMtxBuf[1] == &J3DModel::sNoUseDrawMtxPtr; }
};

namespace {

using Acc = PcFps60ModelAccess;

constexpr f32 kMaxMove = 300.0f;     // a matrix moving further between two frames: a cut
constexpr f32 kMinCosRoot = 0.70710678f; // ... or its first (root) matrix turning more than 45 degrees
constexpr f32 kMinCosJoint = -0.5f;      // ... or another one more than 120 degrees (a fast limb, e.g. an
                                         // arm swinging 60-90 degrees a frame, is blended; past 120 the
                                         // blend of the two turns is ambiguous)
constexpr int kArenas = 3;

// How a model's shapes get their matrices (the model data's load type, J3DModelData flags & 0xF0;
// J3DModelData.h names 0x10 "ConcatView" and 0x20 "NoUseDrawMtx", J3DShapeFactory.cpp calls them
// Imm and ConcatView, which is what the shapes do).
enum Kind : u8 {
    kKindNone, // not blended
    kKindDraw, // 0x00: view-space draw (+ normal) matrices from viewCalc, indexed loads at paint time
    kKindImm,  // 0x10: view-space draw matrices from viewCalc, immediate loads (normals from them)
    kKindAnm,  // 0x20: world-space joint / envelope matrices concatenated with the view at paint time
    kKindSkin, // CPU skinning (J3DSkinDeform: world-space vertices made in calc, drawn with the view as
               // the position matrix): moved rigidly with its root joint (deformation stays N+1's)
};

enum Skip {
    kSkipNoPrev,    // not captured in the previous draw pass (first frame shown, hidden, re-made)
    kSkipOtherView, // view-space matrices made with another view (a shadow's light view)
    kSkipChanged,   // the matrices changed after the capture
    kSkipJump,      // moved / turned over the thresholds
    kSkipKind,      // static (no animation), CPU skinning in calc mode 2
    kSkipCount
};
const char* const kSkipNames[kSkipCount] = {"new", "other view", "changed", "jump", "kind"};

struct Rec {
    J3DModel* model;
    J3DModelData* data;
    Kind kind;
    bool dead;    // re-made at this address after the capture
    bool hasBase; // kKindAnm in calc mode 2: mViewBaseMtx (view * base) blended too
    bool hasNrm;  // kKindDraw: normal matrices
    u32 viewNo;
    u32 count;  // matrices: draw (kKindDraw/Imm) or joints + envelopes (kKindAnm)
    u32 nodes;  // kKindAnm: joint matrices (the envelopes follow)
    u32 off;    // into Cap::mtx
    u32 nrmOff; // into Cap::nrm
    Mtx view;   // j3dSys's view at its last viewCalc
    Mtx base;   // mViewBaseMtx (hasBase)
};

struct Cap {
    unsigned int frame = 0;
    Rec* recs = nullptr;
    u32 nrec = 0, caprec = 0;
    Mtx* mtx = nullptr;
    u32 nmtx = 0, capmtx = 0;
    Mtx33* nrm = nullptr;
    u32 nnrm = 0, capnrm = 0;
    u32* slots = nullptr; // open addressing: rec index + 1, 0 = empty
    u32 capslot = 0;
};

Cap sCaps[2];
Cap* sCur = &sCaps[0];
Cap* sPrev = &sCaps[1];
bool sInDraw = false;
bool sPainting = false;

struct Arena {
    float* data = nullptr;
    size_t cap = 0; // floats
};
Arena sArenas[kArenas];
unsigned int sArenaNext = 0;

struct Restore {
    J3DModel* model;
    Kind kind;
    bool hasBase;
    u32 viewNo;
    Mtx* draw;
    Mtx33* nrm;
    Mtx* nodes;
    Mtx* weights;
    Mtx base;
};
Restore* sRestore = nullptr;
u32 sNRestore = 0, sCapRestore = 0;
// kKindSkin: shape packets whose base matrix pointer (the position matrix of CPU-skinned shapes)
// points at the arena in paint B
struct RestorePacket {
    J3DShapePacket* packet;
    Mtx* base;
};
RestorePacket* sRestorePk = nullptr;
u32 sNRestorePk = 0, sCapRestorePk = 0;

// paint B's blend (pc_fps60_models_paint_begin until _end): for pc_fps60_packet_mtx
bool sBlendActive = false;
f32 sBlendT = 0.5f;
Mtx sK;
// pc_fps60_packet_mtx: matrices custom packets loaded in paint A (the draw pass before's), by packet
struct PacketMtx {
    const void* key;
    unsigned int frame; // pc_frame_count() at paint A
    Mtx m;
};
constexpr int kPacketMtxMax = 64;
PacketMtx sPacketMtx[kPacketMtxMax];
int sNPacketMtx = 0;

// stats since the last perf line
uint64_t sCaptureNs = 0, sBlendNs = 0;
unsigned long sCapturedModels = 0, sCapturedMtx = 0, sBlendedModels = 0, sBlendedMtx = 0, sPaints = 0;
unsigned long sSkips[kSkipCount] = {};
// for COS_FPS60_MODELS_LOG
unsigned long sLogSkips[kSkipCount] = {};
unsigned long sLogKinds[5] = {};
unsigned long sLogBlended = 0, sLogPaints = 0;
uint64_t sLastLogNs = 0;

uint64_t nowNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

bool logOn() {
    static const bool on = [] {
        const char* v = getenv("COS_FPS60_MODELS_LOG");
        return v != nullptr && v[0] == '1';
    }();
    return on;
}

template <typename T> void grow(T*& p, u32& cap, u32 need) {
    if (need <= cap) {
        return;
    }
    u32 n = cap != 0 ? cap : 256;
    while (n < need) {
        n *= 2;
    }
    void* q = realloc(p, (size_t)n * sizeof(T));
    if (q == nullptr) {
        abort();
    }
    p = (T*)q;
    cap = n;
}

u32 hashPtr(const void* p, u32 mask) {
    uint64_t x = (uint64_t)(uintptr_t)p;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    return (u32)x & mask;
}

void rehash(Cap& c, u32 capslot) {
    free(c.slots);
    c.slots = (u32*)calloc(capslot, sizeof(u32));
    if (c.slots == nullptr) {
        abort();
    }
    c.capslot = capslot;
    const u32 mask = capslot - 1;
    for (u32 i = 0; i < c.nrec; i++) {
        u32 h = hashPtr(c.recs[i].model, mask);
        while (c.slots[h] != 0) {
            h = (h + 1) & mask;
        }
        c.slots[h] = i + 1;
    }
}

Rec* find(Cap& c, const J3DModel* m) {
    if (c.capslot == 0) {
        return nullptr;
    }
    const u32 mask = c.capslot - 1;
    for (u32 h = hashPtr(m, mask);; h = (h + 1) & mask) {
        const u32 s = c.slots[h];
        if (s == 0) {
            return nullptr;
        }
        if (c.recs[s - 1].model == m) {
            return &c.recs[s - 1];
        }
    }
}

Rec* insert(Cap& c, J3DModel* m) {
    if ((c.nrec + 1) * 2 > c.capslot) {
        rehash(c, c.capslot != 0 ? c.capslot * 2 : 1024);
    }
    grow(c.recs, c.caprec, c.nrec + 1);
    Rec* r = &c.recs[c.nrec++];
    memset(r, 0, sizeof(*r));
    r->model = m;
    const u32 mask = c.capslot - 1;
    u32 h = hashPtr(m, mask);
    while (c.slots[h] != 0) {
        h = (h + 1) & mask;
    }
    c.slots[h] = c.nrec;
    return r;
}

void clearCap(Cap& c) {
    c.nrec = 0;
    c.nmtx = 0;
    c.nnrm = 0;
    if (c.slots != nullptr) {
        memset(c.slots, 0, c.capslot * sizeof(u32));
    }
}

Kind kindOf(J3DModel* m) {
    J3DModelData* d = Acc::data(m);
    if (d == nullptr) {
        return kKindNone;
    }
    if ((Acc::flags(m) & (J3DMdlFlag_SkinPosCpu | J3DMdlFlag_SkinNrmCpu)) != 0) {
        // CPU skinning: the vertices themselves are made by the CPU (world space, J3DSkinDeform) and
        // drawn with the packets' base matrix (the view in calc mode 0/1)
        return Acc::nodes(m) != nullptr && d->getJointNum() != 0 && (Acc::flags(m) & 0x03) != 2 &&
                       m->getShapePacketArray() != nullptr
                   ? kKindSkin
                   : kKindNone;
    }
    switch (d->getFlag() & 0xF0) {
    case 0x20:
        return Acc::nodes(m) != nullptr && d->getJointNum() != 0 ? kKindAnm : kKindNone;
    case 0x10:
    case 0x00:
        // a static model (no animation) has no draw matrices of its own
        if (Acc::noUseDrawMtx(m) || Acc::drawArr(m) == nullptr || d->getDrawMtxNum() == 0) {
            return kKindNone;
        }
        return (d->getFlag() & 0xF0) == 0x10 ? kKindImm : kKindDraw;
    default:
        return kKindNone;
    }
}

bool sameMtx(const Mtx a, const Mtx b) { return memcmp(a, b, sizeof(Mtx)) == 0; }

// out = a * b (3x4 affine)
void concat(const Mtx a, const Mtx b, Mtx out) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            f32 v = a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c];
            if (c == 3) {
                v += a[r][3];
            }
            out[r][c] = v;
        }
    }
}

// Column j of a 3x3 (rows `stride` floats apart) blended at t and rescaled to the lerped length (a
// turn that keeps the scale); false when the column turned more than 45 degrees.
inline bool blendColumn(const f32* a, const f32* b, f32* out, int stride, int j, f32 t, f32 minCos) {
    const f32 ax = a[j], ay = a[stride + j], az = a[2 * stride + j];
    const f32 bx = b[j], by = b[stride + j], bz = b[2 * stride + j];
    f32 vx = ax + (bx - ax) * t, vy = ay + (by - ay) * t, vz = az + (bz - az) * t;
    const f32 la = std::sqrt(ax * ax + ay * ay + az * az), lb = std::sqrt(bx * bx + by * by + bz * bz);
    if (la > 1e-6f && lb > 1e-6f) {
        if (ax * bx + ay * by + az * bz < minCos * la * lb) {
            return false;
        }
        const f32 lv = std::sqrt(vx * vx + vy * vy + vz * vz);
        if (lv > 1e-12f) {
            const f32 k = (la + (lb - la) * t) / lv;
            vx *= k;
            vy *= k;
            vz *= k;
        }
    }
    out[j] = vx;
    out[stride + j] = vy;
    out[2 * stride + j] = vz;
    return true;
}

// out = blend(a, b, t) of two 3x4 matrices; false past the move / turn thresholds.
inline bool blendMtx(const Mtx a, const Mtx b, Mtx out, f32 t, bool keepRotation, f32 minCos) {
    const f32 dx = b[0][3] - a[0][3], dy = b[1][3] - a[1][3], dz = b[2][3] - a[2][3];
    if (dx * dx + dy * dy + dz * dz > kMaxMove * kMaxMove) {
        return false;
    }
    for (int row = 0; row < 3; row++) {
        out[row][3] = a[row][3] + (b[row][3] - a[row][3]) * t;
    }
    if (keepRotation) {
        for (int row = 0; row < 3; row++) {
            out[row][0] = b[row][0];
            out[row][1] = b[row][1];
            out[row][2] = b[row][2];
        }
        return true;
    }
    return blendColumn(&a[0][0], &b[0][0], &out[0][0], 4, 0, t, minCos) &&
           blendColumn(&a[0][0], &b[0][0], &out[0][0], 4, 1, t, minCos) &&
           blendColumn(&a[0][0], &b[0][0], &out[0][0], 4, 2, t, minCos);
}

// A view-space billboard draw matrix (calcBBoard: it faces the camera): its rotation is not blended.
bool isBillboard(J3DModelData* d, u16 i) {
    if (!d->checkBBoardFlag() || d->getDrawMtxFlag(i) != 0) {
        return false;
    }
    const u8 type = d->getJointNodePointer(d->getDrawMtxIndex(i))->getMtxType();
    return type == J3DJntMtxType_BBoard || type == J3DJntMtxType_YBBoard;
}

float* arenaFor(size_t floats) {
    Arena& a = sArenas[sArenaNext];
    sArenaNext = (sArenaNext + 1) % kArenas;
    if (floats > a.cap) {
        size_t n = a.cap != 0 ? a.cap : 4096;
        while (n < floats) {
            n *= 2;
        }
        // this arena's last paint B has been processed (see the top): its memory may move
        void* q = realloc(a.data, n * sizeof(float));
        if (q == nullptr) {
            abort();
        }
        a.data = (float*)q;
        a.cap = n;
    }
    return a.data;
}

void maybeLog() {
    if (!logOn()) {
        return;
    }
    const uint64_t now = nowNs();
    if (now - sLastLogNs < 2000000000ull) {
        return;
    }
    sLastLogNs = now;
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[cos] fps60 models: frame %u: %lu paint(s) B: %lu blended", pc_frame_count() + 1,
                     sLogPaints, sLogBlended);
    for (int i = 0; i < kSkipCount; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, ", %s %lu", kSkipNames[i], sLogSkips[i]);
        sLogSkips[i] = 0;
    }
    snprintf(buf + n, sizeof(buf) - n, " (captured: draw %lu, imm %lu, anm %lu, cpu skin %lu, other %lu)\n",
             sLogKinds[kKindDraw], sLogKinds[kKindImm], sLogKinds[kKindAnm], sLogKinds[kKindSkin], sLogKinds[kKindNone]);
    memset(sLogKinds, 0, sizeof(sLogKinds));
    pc::writef(STDERR_FILENO, "%s", buf);
    sLogPaints = sLogBlended = 0;
}

} // namespace

void pc_fps60_painting(int on) { sPainting = on != 0; }

void pc_fps60_model_init(J3DModel* model) {
    for (Cap& c : sCaps) {
        if (Rec* r = find(c, model)) {
            r->dead = true;
        }
    }
}

void pc_fps60_models_draw_begin(void) {
    if (!pc_fps60_test()) {
        sInDraw = false;
        return;
    }
    // this draw pass's captures: the last ones become "prev"
    Cap* t = sPrev;
    sPrev = sCur;
    sCur = t;
    clearCap(*sCur);
    sCur->frame = pc_frame_count() + 1;
    sInDraw = true;
}

void pc_fps60_model_viewcalc(J3DModel* model) {
    if (!sInDraw || sPainting) {
        return;
    }
    Rec* r = find(*sCur, model);
    if (r == nullptr) {
        r = insert(*sCur, model);
    }
    MTXCopy(j3dSys.getViewMtx(), r->view);
}

void pc_fps60_models_draw_end(void) {
    if (!sInDraw) {
        return;
    }
    sInDraw = false;
    const uint64_t t0 = nowNs();
    Cap& c = *sCur;
    for (u32 i = 0; i < c.nrec; i++) {
        Rec& r = c.recs[i];
        J3DModel* m = r.model;
        r.kind = r.dead ? kKindNone : kindOf(m);
        sLogKinds[r.kind]++;
        if (r.kind == kKindNone) {
            static unsigned int sOtherLogged = 0;
            if (logOn() && !r.dead && sOtherLogged < 10 && Acc::data(m) != nullptr) {
                sOtherLogged++;
                pc::writef(STDERR_FILENO,
                           "[cos] fps60 models: frame %u: model %p not blended: model flags 0x%x, data flags 0x%x, "
                           "%u joints, %u draw matrices\n",
                           c.frame, (void*)m, Acc::flags(m), Acc::data(m)->getFlag(), Acc::data(m)->getJointNum(),
                           Acc::data(m)->getDrawMtxNum());
            }
            continue;
        }
        J3DModelData* d = Acc::data(m);
        r.data = d;
        r.viewNo = Acc::viewNo(m);
        if (r.kind == kKindSkin) {
            // the root joint's world matrix
            r.nodes = r.count = 1;
            grow(c.mtx, c.capmtx, c.nmtx + 1);
            r.off = c.nmtx;
            MTXCopy(Acc::nodes(m)[0], c.mtx[c.nmtx]);
            c.nmtx += 1;
        } else if (r.kind == kKindAnm) {
            r.nodes = d->getJointNum();
            const u32 weights = Acc::weights(m) != nullptr ? d->getWEvlpMtxNum() : 0;
            r.count = r.nodes + weights;
            grow(c.mtx, c.capmtx, c.nmtx + r.count);
            r.off = c.nmtx;
            memcpy(c.mtx + c.nmtx, Acc::nodes(m), r.nodes * sizeof(Mtx));
            if (weights != 0) {
                memcpy(c.mtx + c.nmtx + r.nodes, Acc::weights(m), weights * sizeof(Mtx));
            }
            c.nmtx += r.count;
            r.hasBase = (Acc::flags(m) & 0x03) == 2;
            if (r.hasBase) {
                MTXCopy(Acc::viewBase(m), r.base);
            }
        } else {
            r.count = d->getDrawMtxNum();
            r.hasNrm = r.kind == kKindDraw && Acc::nrmArr(m) != nullptr;
            grow(c.mtx, c.capmtx, c.nmtx + r.count);
            r.off = c.nmtx;
            memcpy(c.mtx + c.nmtx, Acc::drawArr(m)[r.viewNo], r.count * sizeof(Mtx));
            c.nmtx += r.count;
            if (r.hasNrm) {
                grow(c.nrm, c.capnrm, c.nnrm + r.count);
                r.nrmOff = c.nnrm;
                memcpy(c.nrm + c.nnrm, Acc::nrmArr(m)[r.viewNo], r.count * sizeof(Mtx33));
                c.nnrm += r.count;
            }
        }
        sCapturedModels++;
        sCapturedMtx += r.count;
    }
    sCaptureNs += nowNs() - t0;
}

void pc_fps60_models_paint_begin(float t, const float viewPrev[3][4], const float viewCur[3][4]) {
    sNRestore = 0;
    sNRestorePk = 0;
    const unsigned int frame = pc_frame_count() + 1;
    if (sCur->frame != frame || sPrev->frame + 1 != frame) {
        return; // no draw pass captured just before (the mode just turned on)
    }
    const uint64_t t0 = nowNs();
    sPaints++;
    sLogPaints++;
    // K = V_cur * V_prev^-1: a view-space matrix of the previous frame seen with this frame's view
    Mtx inv, K;
    if (!MTXInverse(viewPrev, inv)) {
        return;
    }
    concat(viewCur, inv, K);
    MTXCopy(K, sK);
    sBlendT = t;
    sBlendActive = true;

    Cap& cur = *sCur;
    Cap& prev = *sPrev;
    // worst case: every captured matrix blended
    float* out = arenaFor((size_t)cur.nmtx * 12 + (size_t)cur.nnrm * 9);
    Mtx* outMtx = (Mtx*)out;
    Mtx33* outNrm = (Mtx33*)(out + (size_t)cur.nmtx * 12);
    u32 usedMtx = 0, usedNrm = 0;

    for (u32 i = 0; i < cur.nrec; i++) {
        Rec& r = cur.recs[i];
        Skip why = kSkipCount;
        Rec* p = nullptr;
        J3DModel* m = r.model;
        const bool viewSpace = r.kind == kKindDraw || r.kind == kKindImm || r.hasBase;
        if (r.kind == kKindNone) {
            why = r.dead ? kSkipNoPrev : kSkipKind;
        } else if (r.dead) {
            why = kSkipNoPrev;
        } else if (viewSpace && !sameMtx(r.view, viewCur)) {
            why = kSkipOtherView;
        } else if ((p = find(prev, m)) == nullptr || p->dead || p->kind != r.kind || p->data != r.data ||
                   p->count != r.count || p->viewNo != r.viewNo || p->hasBase != r.hasBase ||
                   p->hasNrm != r.hasNrm) {
            why = kSkipNoPrev;
        } else if (viewSpace && !sameMtx(p->view, viewPrev)) {
            why = kSkipOtherView;
        } else if (r.kind == kKindAnm || r.kind == kKindSkin
                       ? (Acc::nodes(m) == nullptr || memcmp(Acc::nodes(m), cur.mtx + r.off, r.nodes * sizeof(Mtx)) != 0 ||
                          (r.count > r.nodes && (Acc::weights(m) == nullptr ||
                                                 memcmp(Acc::weights(m), cur.mtx + r.off + r.nodes,
                                                        (r.count - r.nodes) * sizeof(Mtx)) != 0)) ||
                          (r.hasBase && !sameMtx(Acc::viewBase(m), r.base)))
                       : (Acc::drawArr(m)[r.viewNo] == nullptr ||
                          memcmp(Acc::drawArr(m)[r.viewNo], cur.mtx + r.off, r.count * sizeof(Mtx)) != 0)) {
            why = kSkipChanged; // something rewrote them after the draw pass: leave them
        }
        if (why != kSkipCount) {
            sSkips[why]++;
            sLogSkips[why]++;
            continue;
        }
        const Mtx* mc = cur.mtx + r.off;
        const Mtx* mp = prev.mtx + p->off;
        Mtx* om = outMtx + usedMtx;
        Mtx33* on = outNrm + usedNrm;
        Mtx base;
        bool ok = true;
        if (r.kind == kKindSkin) {
            // V_cur * R_t * R_cur^-1: the skinned vertices (made for R_cur) moved with the root to R_t
            Mtx rt, inv2, o;
            ok = blendMtx(mp[0], mc[0], rt, t, false, kMinCosRoot) && MTXInverse(mc[0], inv2);
            if (ok) {
                concat(rt, inv2, o);
                concat(viewCur, o, om[0]);
            }
        } else if (r.kind == kKindAnm) {
            // world space: lerp(W_prev, W_cur); the view is applied at paint time (V_cur, then step B)
            for (u32 k = 0; k < r.count && ok; k++) {
                ok = blendMtx(mp[k], mc[k], om[k], t, false, k == 0 ? kMinCosRoot : kMinCosJoint);
            }
            if (ok && r.hasBase) {
                Mtx a;
                concat(K, p->base, a);
                ok = blendMtx(a, r.base, base, t, false, kMinCosRoot);
            }
        } else {
            // view space: lerp(K * D_prev, D_cur) = V_cur * lerp(W_prev, W_cur)
            const Mtx33* nc = cur.nrm + r.nrmOff;
            const Mtx33* np = prev.nrm + p->nrmOff;
            for (u32 k = 0; k < r.count && ok; k++) {
                Mtx a;
                concat(K, mp[k], a);
                const bool bb = isBillboard(r.data, (u16)k);
                ok = blendMtx(a, mc[k], om[k], t, bb, k == 0 ? kMinCosRoot : kMinCosJoint);
                if (!ok || !r.hasNrm) {
                    continue;
                }
                if (bb) {
                    memcpy(on[k], nc[k], sizeof(Mtx33));
                    continue;
                }
                // normals: K's rotation * N_prev, blended with N_cur
                Mtx33 na;
                for (int row = 0; row < 3; row++) {
                    for (int col = 0; col < 3; col++) {
                        na[row][col] =
                            K[row][0] * np[k][0][col] + K[row][1] * np[k][1][col] + K[row][2] * np[k][2][col];
                    }
                }
                for (int j = 0; j < 3 && ok; j++) {
                    ok = blendColumn(&na[0][0], &nc[k][0][0], &on[k][0][0], 3, j, t, k == 0 ? kMinCosRoot : kMinCosJoint);
                }
            }
        }
        if (!ok && logOn()) {
            static unsigned int sJumpLogged = 0, sJumpFrame = 0;
            if (frame != sJumpFrame) {
                sJumpFrame = frame;
            }
            if (sJumpLogged < 40) {
                sJumpLogged++;
                // which matrix: the first one past a threshold
                for (u32 k = 0; k < r.count; k++) {
                    Mtx a, o;
                    if (r.kind == kKindAnm || r.kind == kKindSkin) {
                        MTXCopy(mp[k], a);
                    } else {
                        concat(K, mp[k], a);
                    }
                    if (!blendMtx(a, mc[k], o, t, false, k == 0 ? kMinCosRoot : kMinCosJoint)) {
                        const f32 dx = mc[k][0][3] - a[0][3], dy = mc[k][1][3] - a[1][3], dz = mc[k][2][3] - a[2][3];
                        pc::writef(STDERR_FILENO,
                                   "[cos] fps60 models: frame %u: model %p (data %p, %u joints, kind %d) matrix %u "
                                   "jumped: moved %.1f, col0 %.3f %.3f %.3f -> %.3f %.3f %.3f\n",
                                   frame, (void*)m, (void*)r.data, r.data->getJointNum(), (int)r.kind, k,
                                   std::sqrt(dx * dx + dy * dy + dz * dz), a[0][0], a[1][0], a[2][0], mc[k][0][0],
                                   mc[k][1][0], mc[k][2][0]);
                        break;
                    }
                }
            }
        }
        if (!ok) {
            sSkips[kSkipJump]++;
            sLogSkips[kSkipJump]++;
            continue;
        }
        grow(sRestore, sCapRestore, sNRestore + 1);
        Restore& s = sRestore[sNRestore++];
        s.model = m;
        s.kind = r.kind;
        s.viewNo = r.viewNo;
        s.hasBase = r.hasBase;
        s.nrm = nullptr;
        if (r.kind == kKindSkin) {
            const u16 shapes = r.data->getShapeNum();
            for (u16 k = 0; k < shapes; k++) {
                J3DShapePacket* pk = m->getShapePacket(k);
                grow(sRestorePk, sCapRestorePk, sNRestorePk + 1);
                sRestorePk[sNRestorePk++] = {pk, pk->getBaseMtxPtr()};
                pk->setBaseMtxPtr(om);
            }
        } else if (r.kind == kKindAnm) {
            s.nodes = Acc::nodes(m);
            s.weights = Acc::weights(m);
            Acc::nodes(m) = om;
            if (r.count > r.nodes) {
                Acc::weights(m) = om + r.nodes;
            }
            if (r.hasBase) {
                MTXCopy(Acc::viewBase(m), s.base);
                MTXCopy(base, Acc::viewBase(m));
            }
        } else {
            s.draw = Acc::drawArr(m)[r.viewNo];
            Acc::drawArr(m)[r.viewNo] = om;
            if (r.hasNrm) {
                s.nrm = Acc::nrmArr(m)[r.viewNo];
                Acc::nrmArr(m)[r.viewNo] = on;
                usedNrm += r.count;
            }
        }
        usedMtx += r.count;
        sBlendedModels++;
        sLogBlended++;
        sBlendedMtx += r.count;
    }
    sBlendNs += nowNs() - t0;
    maybeLog();
}

void pc_fps60_models_paint_end(void) {
    const uint64_t t0 = nowNs();
    sBlendActive = false;
    // in reverse order (a pointer array shared by two models would end with the first one's value)
    for (u32 i = sNRestore; i-- > 0;) {
        Restore& s = sRestore[i];
        J3DModel* m = s.model;
        if (s.kind == kKindAnm) {
            Acc::nodes(m) = s.nodes;
            Acc::weights(m) = s.weights;
            if (s.hasBase) {
                MTXCopy(s.base, Acc::viewBase(m));
            }
        } else if (s.kind != kKindSkin) {
            Acc::drawArr(m)[s.viewNo] = s.draw;
            if (s.kind == kKindDraw && Acc::nrmArr(m) != nullptr && s.nrm != nullptr) {
                Acc::nrmArr(m)[s.viewNo] = s.nrm;
            }
        }
    }
    sNRestore = 0;
    for (u32 i = sNRestorePk; i-- > 0;) {
        sRestorePk[i].packet->setBaseMtxPtr(sRestorePk[i].base);
    }
    sNRestorePk = 0;
    sBlendNs += nowNs() - t0;
}

void pc_fps60_models_stats(char* out, unsigned long size, double frames) {
    if (frames <= 0) {
        frames = 1;
    }
    const double paints = sPaints != 0 ? (double)sPaints : 1.0;
    snprintf(out, size,
             ", models: %.0f captured (%.0f mtx), %.0f blended (%.0f mtx) per paint B, held new %.1f view %.1f "
             "changed %.1f jump %.1f kind %.1f, capture %.3f ms, blend %.3f ms",
             sCapturedModels / frames, sCapturedMtx / frames, sBlendedModels / paints, sBlendedMtx / paints,
             sSkips[kSkipNoPrev] / paints, sSkips[kSkipOtherView] / paints, sSkips[kSkipChanged] / paints,
             sSkips[kSkipJump] / paints, sSkips[kSkipKind] / paints, sCaptureNs / frames / 1e6,
             sBlendNs / frames / 1e6);
    sCaptureNs = sBlendNs = 0;
    sCapturedModels = sCapturedMtx = sBlendedModels = sBlendedMtx = sPaints = 0;
    for (unsigned long& s : sSkips) {
        s = 0;
    }
}

void pc_fps60_packet_mtx(const void* key, const float m[3][4], float out[3][4]) {
    MTXCopy(m, out);
    if (!pc_fps60_test() || !sPainting) {
        return;
    }
    const unsigned int frame = pc_frame_count();
    if (!pc_paint_is_extra()) {
        // paint A: this matrix is the draw pass before's, "prev" for this frame's paint B
        int i = 0;
        while (i < sNPacketMtx && sPacketMtx[i].key != key) {
            i++;
        }
        if (i == sNPacketMtx) {
            if (sNPacketMtx == kPacketMtxMax) {
                // the oldest entries go (a stale one is never used: its frame does not match)
                memmove(&sPacketMtx[0], &sPacketMtx[1], sizeof(PacketMtx) * (kPacketMtxMax - 1));
                i = kPacketMtxMax - 1;
            } else {
                sNPacketMtx++;
            }
        }
        sPacketMtx[i].key = key;
        sPacketMtx[i].frame = frame;
        MTXCopy(m, sPacketMtx[i].m);
        return;
    }
    if (!sBlendActive) {
        return;
    }
    for (int i = 0; i < sNPacketMtx; i++) {
        if (sPacketMtx[i].key == key) {
            if (sPacketMtx[i].frame == frame) {
                Mtx a, o;
                concat(sK, sPacketMtx[i].m, a);
                if (blendMtx(a, m, o, sBlendT, false, kMinCosRoot)) {
                    MTXCopy(o, out);
                }
            }
            return;
        }
    }
}
