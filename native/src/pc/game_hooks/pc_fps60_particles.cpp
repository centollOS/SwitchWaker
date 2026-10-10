// COS_FPS60_TEST step D (docs/FPS60_PLAN.md, "Step D"): paint B draws the JPA particles (the boat's
// wake and bow waves, splashes, dust, fire, sparkles, the 2D effects) at t between where the draw pass
// before left them (the paint A just shown) and where this frame's left them.
//
// JPA's calc runs in the draw pass (dComIfGp_particle_calc3D/2D/Menu, d_s_play.cpp), its draw in the
// painter; positions, sizes, colours and rotations are the particles' own fields (JPABaseParticle:
// mGlobalPosition, JPADrawParams), read by the draw visitors and the emitter / particle callbacks
// (dPa_waveEcallBack's bow waves, dPa_trackEcallBack's wake, dPa_stripesEcallBack, ...) at paint time.
//
// - Capture (pc_fps60_particles_capture, from pc_fps60_models_draw_end, i.e. the draw pass's end):
//   every live particle of the one JPAEmitterManager (registered by its constructor with its particle
//   and emitter pools) is copied into a host table indexed by its pool slot: position, age
//   (mCurFrame), rotation angle, scale, alpha, colours, emitter; every emitter (pool slot): global
//   translation and rotation, tick, data. The last capture becomes "prev". Host memory only.
// - Identity without touching the game's structures: a slot is the same particle in both captures
//   when it belongs to the same emitter and its age went up by one (or stayed, the calc skipped:
//   pause, StopCalc); an emitter when its data is the same and its tick went up by one or stayed.
//   A particle born in this draw pass (age 0, no "prev") moves with its emitter: its position gets
//   the emitter's translation step back (E_t - E_cur), so the wake's and the bow waves' newest
//   particles stay at the boat's blended position instead of half a step ahead.
// - Paint B (JPAEmitterManager::draw, per emitter, between pc_fps60_particles_emitter_begin/_end):
//   the emitter's translation, rotation and tick (its frame: texture scrolls at draw time) and every
//   particle's fields are overwritten with the
//   blend at t (position lerped, angle along the short way, scale, alpha and colours lerped; a jump
//   over 300 units or a particle whose live values differ from the capture is left as it is) and
//   put back after the emitter's draw. JPA draws with immediate vertices (GXPosition3f32), so
//   nothing in the GX stream points at the particles: restoring right away is safe for Aurora's
//   FIFO worker, and the game's state after paint B is unchanged (COS_PAINT_PURITY).
// - dPa_waveEcallBack::draw (the bow waves' fan) uses the emitter axes its executeAfter kept from
//   calc: pc_fps60_particles_emitter_turn turns its fan-centre offset by the emitter's blended turn.
// - Active only in a paint B whose camera is not cut (pc_fps60_particles_paint_begin from
//   pc_fps60_view_begin), for the whole paint (3D and 2D groups).
//
// COS_FPS60_PARTICLES=0 turns it off (particles at N+1 in paint B, as in step C).
#include "JSystem/JSystem.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "pc_internal.h"

#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAEmitterManager.h"
#include "JSystem/JParticle/JPAParticle.h"
#include "dolphin/mtx/mtx.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

struct PcFps60JpaAccess {
    static JSUList<JPABaseParticle>& active(JPABaseEmitter* e) { return e->mActiveParticles; }
    static JSUList<JPABaseParticle>& children(JPABaseEmitter* e) { return e->mChildParticles; }
    static JGeometry::TVec3<f32>& trans(JPABaseEmitter* e) { return e->mGlobalTranslation; }
    static Mtx& rot(JPABaseEmitter* e) { return *(Mtx*)&e->mGlobalRotation; }
    static f32 tick(JPABaseEmitter* e) { return e->mTick.getFrame(); }
    static void setTick(JPABaseEmitter* e, f32 f) { e->mTick.setFrame(f); }
    static const void* data(JPABaseEmitter* e) { return e->mpDataLinkInfo; }
};

namespace {

using Acc = PcFps60JpaAccess;

constexpr f32 kMaxMove = 300.0f; // a particle or emitter moving further in one frame: not blended

bool enabled() {
    static const bool on = [] {
        const char* v = getenv("COS_FPS60_PARTICLES");
        return !(v != nullptr && v[0] == '0');
    }();
    return on;
}

uint64_t nowNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

struct PtclSnap {
    unsigned int frame; // capture frame; anything else: not captured
    const JPABaseEmitter* emitter;
    f32 age;
    JGeometry::TVec3<f32> pos;
    f32 scaleX, scaleY, alpha;
    GXColor prm, env;
    u16 angle;
};

struct EmtrSnap {
    unsigned int frame;
    const void* data;
    f32 tick;
    JGeometry::TVec3<f32> trans;
    Mtx rot;
};

struct Snap {
    unsigned int frame = 0;
    PtclSnap* ptcl = nullptr;
    EmtrSnap* emtr = nullptr;
};

JPAEmitterManager* sMgr = nullptr;
JPABaseParticle* sPtclBase = nullptr;
u32 sPtclNum = 0;
JPABaseEmitter* sEmtrBase = nullptr;
u32 sEmtrNum = 0;
Snap sSnaps[2];
Snap* sCur = &sSnaps[0];
Snap* sPrev = &sSnaps[1];

// paint B
bool sOn = false;
unsigned int sOnFrame = 0;
f32 sT = 0.5f;

// the emitter being drawn: what to put back
struct PtclRestore {
    JPABaseParticle* p;
    JGeometry::TVec3<f32> pos;
    f32 scaleX, scaleY, alpha;
    GXColor prm, env;
    u16 angle;
};
PtclRestore* sRestore = nullptr;
u32 sNRestore = 0, sCapRestore = 0;
JPABaseEmitter* sEmtr = nullptr; // emitter_begin .. _end
bool sEmtrMoved = false;
unsigned int sBornActive = 0; // newborns of its active list moved back with it
bool sEmtrTicked = false; // its tick set to the blend (texture scrolls read it at draw time)
f32 sEmtrTick = 0.0f;
JGeometry::TVec3<f32> sEmtrTrans;
Mtx sEmtrRot, sEmtrTurn; // the emitter's own rotation, and R_t * R_cur^-1

// stats since the last perf line
uint64_t sCaptureNs = 0, sBlendNs = 0;
unsigned long sCaptured = 0, sBlended = 0, sBorn = 0, sHeld = 0, sEmitters = 0, sPaints = 0;

int ptclIndex(const JPABaseParticle* p) {
    if (p < sPtclBase || p >= sPtclBase + sPtclNum) {
        return -1;
    }
    return (int)(p - sPtclBase);
}

int emtrIndex(const JPABaseEmitter* e) {
    if (e < sEmtrBase || e >= sEmtrBase + sEmtrNum) {
        return -1;
    }
    return (int)(e - sEmtrBase);
}

void capturePtcl(Snap& s, JPABaseEmitter* e, JPABaseParticle* p) {
    const int i = ptclIndex(p);
    if (i < 0) {
        return;
    }
    PtclSnap& c = s.ptcl[i];
    c.frame = s.frame;
    c.emitter = e;
    c.age = p->mCurFrame;
    c.pos = p->mGlobalPosition;
    const JPADrawParams& d = p->mDrawParams;
    c.scaleX = d.mScaleX;
    c.scaleY = d.mScaleY;
    c.alpha = d.mAlphaOut;
    c.prm = d.mPrmColor;
    c.env = d.mEnvColor;
    c.angle = d.mRotateAngle;
    sCaptured++;
}

f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

u8 lerpu8(u8 a, u8 b, f32 t) { return (u8)((f32)a + ((f32)b - (f32)a) * t + 0.5f); }

GXColor lerpColor(GXColor a, GXColor b, f32 t) {
    return GXColor{lerpu8(a.r, b.r, t), lerpu8(a.g, b.g, t), lerpu8(a.b, b.b, t), lerpu8(a.a, b.a, t)};
}

bool sameColor(GXColor a, GXColor b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

f32 dist2(const JGeometry::TVec3<f32>& a, const JGeometry::TVec3<f32>& b) {
    const f32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// The particle's live fields still those of the capture (nothing rewrote them after the draw pass).
bool unchanged(const PtclSnap& c, JPABaseParticle* p) {
    const JPADrawParams& d = p->mDrawParams;
    return c.age == p->mCurFrame && c.pos.x == p->mGlobalPosition.x && c.pos.y == p->mGlobalPosition.y &&
           c.pos.z == p->mGlobalPosition.z && c.scaleX == d.mScaleX && c.scaleY == d.mScaleY &&
           c.alpha == d.mAlphaOut && sameColor(c.prm, d.mPrmColor) && sameColor(c.env, d.mEnvColor) &&
           c.angle == d.mRotateAngle;
}

// The particle's fields kept to be put back after the emitter's draw.
void grow(JPABaseParticle* p);

void blendList(JSUList<JPABaseParticle>& list, JPABaseEmitter* e) {
    for (JSULink<JPABaseParticle>* l = list.getFirst(); l != nullptr; l = l->getNext()) {
        JPABaseParticle* p = l->getObject();
        const int i = ptclIndex(p);
        if (i < 0) {
            continue;
        }
        const PtclSnap& c = sCur->ptcl[i];
        if (c.frame != sCur->frame || c.emitter != e || !unchanged(c, p)) {
            sHeld++;
            continue;
        }
        const PtclSnap& q = sPrev->ptcl[i];
        const bool same = q.frame == sPrev->frame && q.emitter == e && (c.age == q.age + 1.0f || c.age == q.age);
        if (!same) {
            // born in this draw pass (created with age -1, aged to 0 by the same calc): moved with its
            // emitter's blended translation
            if (c.age <= 0.0f && sEmtrMoved) {
                if (&list == &Acc::active(e)) {
                    sBornActive++;
                }
                grow(p);
                const JGeometry::TVec3<f32>& et = Acc::trans(e);
                p->mGlobalPosition.x += et.x - sEmtrTrans.x;
                p->mGlobalPosition.y += et.y - sEmtrTrans.y;
                p->mGlobalPosition.z += et.z - sEmtrTrans.z;
                sBorn++;
            } else {
                sHeld++;
            }
            continue;
        }
        if (dist2(q.pos, c.pos) > kMaxMove * kMaxMove) {
            sHeld++;
            continue;
        }
        grow(p);
        const f32 t = sT;
        p->mGlobalPosition.set(lerpf(q.pos.x, c.pos.x, t), lerpf(q.pos.y, c.pos.y, t), lerpf(q.pos.z, c.pos.z, t));
        JPADrawParams& d = p->mDrawParams;
        d.mScaleX = lerpf(q.scaleX, c.scaleX, t);
        d.mScaleY = lerpf(q.scaleY, c.scaleY, t);
        d.mAlphaOut = lerpf(q.alpha, c.alpha, t);
        d.mPrmColor = lerpColor(q.prm, c.prm, t);
        d.mEnvColor = lerpColor(q.env, c.env, t);
        d.mRotateAngle = (u16)(q.angle + (s16)((f32)(s16)(c.angle - q.angle) * t));
        sBlended++;
    }
}

void restoreAll() {
    for (u32 k = sNRestore; k-- > 0;) {
        PtclRestore& r = sRestore[k];
        JPABaseParticle* p = r.p;
        p->mGlobalPosition = r.pos;
        JPADrawParams& d = p->mDrawParams;
        d.mScaleX = r.scaleX;
        d.mScaleY = r.scaleY;
        d.mAlphaOut = r.alpha;
        d.mPrmColor = r.prm;
        d.mEnvColor = r.env;
        d.mRotateAngle = r.angle;
    }
    sNRestore = 0;
}

void grow(JPABaseParticle* p) {
    if (sNRestore == sCapRestore) {
        const u32 n = sCapRestore != 0 ? sCapRestore * 2 : 512;
        void* q = realloc(sRestore, (size_t)n * sizeof(PtclRestore));
        if (q == nullptr) {
            abort();
        }
        sRestore = (PtclRestore*)q;
        sCapRestore = n;
    }
    PtclRestore& r = sRestore[sNRestore++];
    r.p = p;
    r.pos = p->mGlobalPosition;
    const JPADrawParams& d = p->mDrawParams;
    r.scaleX = d.mScaleX;
    r.scaleY = d.mScaleY;
    r.alpha = d.mAlphaOut;
    r.prm = d.mPrmColor;
    r.env = d.mEnvColor;
    r.angle = d.mRotateAngle;
}

} // namespace

void pc_fps60_particles_register(JPAEmitterManager* mgr, JPABaseParticle* ptcls, unsigned int nptcl,
                                 JPABaseEmitter* emtrs, unsigned int nemtr) {
    sMgr = mgr;
    sPtclBase = ptcls;
    sPtclNum = nptcl;
    sEmtrBase = emtrs;
    sEmtrNum = nemtr;
    for (Snap& s : sSnaps) {
        free(s.ptcl);
        free(s.emtr);
        s.ptcl = (PtclSnap*)calloc(nptcl != 0 ? nptcl : 1, sizeof(PtclSnap));
        s.emtr = (EmtrSnap*)calloc(nemtr != 0 ? nemtr : 1, sizeof(EmtrSnap));
        if (s.ptcl == nullptr || s.emtr == nullptr) {
            abort();
        }
        s.frame = 0;
    }
}

void pc_fps60_particles_capture(void) {
    if (!pc_fps60_test() || !enabled() || sMgr == nullptr) {
        return;
    }
    const uint64_t t0 = nowNs();
    Snap* t = sPrev;
    sPrev = sCur;
    sCur = t;
    Snap& s = *sCur;
    // frame numbers are never 0, so a zeroed (calloc'd) entry never matches
    s.frame = pc_frame_count() + 1;
    for (int g = 0; g < 16; g++) {
        for (JSULink<JPABaseEmitter>* l = sMgr->mEmtrGroup[g].getFirst(); l != nullptr; l = l->getNext()) {
            JPABaseEmitter* e = l->getObject();
            const int ei = emtrIndex(e);
            if (ei >= 0) {
                EmtrSnap& c = s.emtr[ei];
                c.frame = s.frame;
                c.data = Acc::data(e);
                c.tick = Acc::tick(e);
                c.trans = Acc::trans(e);
                MTXCopy(Acc::rot(e), c.rot);
            }
            for (JSULink<JPABaseParticle>* p = Acc::active(e).getFirst(); p != nullptr; p = p->getNext()) {
                capturePtcl(s, e, p->getObject());
            }
            for (JSULink<JPABaseParticle>* p = Acc::children(e).getFirst(); p != nullptr; p = p->getNext()) {
                capturePtcl(s, e, p->getObject());
            }
        }
    }
    sCaptureNs += nowNs() - t0;
}

void pc_fps60_particles_paint_begin(float t) {
    sOn = enabled() && sMgr != nullptr && sCur->frame == pc_frame_count() + 1 && sPrev->frame + 1 == sCur->frame;
    sOnFrame = pc_frame_count() + 1;
    sT = t;
    if (sOn) {
        sPaints++;
    }
}

int pc_fps60_particles_emitter_begin(JPABaseEmitter* e) {
    if (!sOn || sOnFrame != pc_frame_count() + 1 || !pc_paint_is_extra()) {
        return 0;
    }
    const uint64_t t0 = nowNs();
    sEmtr = e;
    sEmtrMoved = false;
    sEmtrTicked = false;
    sBornActive = 0;
    sNRestore = 0;
    sEmitters++;
    // the emitter's translation and rotation (callbacks, the directional shapes' axis, newborns)
    const int ei = emtrIndex(e);
    if (ei >= 0) {
        const EmtrSnap& c = sCur->emtr[ei];
        const EmtrSnap& q = sPrev->emtr[ei];
        if (c.frame == sCur->frame && q.frame == sPrev->frame && c.data == Acc::data(e) && q.data == c.data &&
            (c.tick == q.tick + 1.0f || c.tick == q.tick) && c.tick == Acc::tick(e) &&
            c.trans.x == Acc::trans(e).x && c.trans.y == Acc::trans(e).y && c.trans.z == Acc::trans(e).z &&
            memcmp(c.rot, Acc::rot(e), sizeof(Mtx)) == 0 && dist2(q.trans, c.trans) <= kMaxMove * kMaxMove) {
            if (c.tick != q.tick) {
                // the emitter's frame (texture scrolls and animations read at draw time, e.g. the
                // wake's dPa_trackEcallBack) a fraction of a frame back
                sEmtrTick = c.tick;
                sEmtrTicked = true;
                Acc::setTick(e, lerpf(q.tick, c.tick, sT));
            }
            Mtx r;
            if (pc_fps60_blend_mtx(q.rot, c.rot, r, sT)) {
                sEmtrTrans = c.trans;
                MTXCopy(c.rot, sEmtrRot);
                Acc::trans(e).set(lerpf(q.trans.x, c.trans.x, sT), lerpf(q.trans.y, c.trans.y, sT),
                                  lerpf(q.trans.z, c.trans.z, sT));
                // the turn R_t * R_cur^-1 (rotation part only; translation zero)
                Mtx inv;
                if (MTXInverse(c.rot, inv)) {
                    MTXConcat(r, inv, sEmtrTurn);
                    sEmtrTurn[0][3] = sEmtrTurn[1][3] = sEmtrTurn[2][3] = 0.0f;
                } else {
                    MTXIdentity(sEmtrTurn);
                }
                for (int row = 0; row < 3; row++) {
                    for (int col = 0; col < 3; col++) {
                        Acc::rot(e)[row][col] = r[row][col];
                    }
                }
                sEmtrMoved = true;
            }
        }
    }
    blendList(Acc::active(e), e);
    blendList(Acc::children(e), e);
    sBlendNs += nowNs() - t0;
    return 1;
}

void pc_fps60_particles_emitter_end(JPABaseEmitter* e) {
    if (sEmtr != e) {
        return;
    }
    const uint64_t t0 = nowNs();
    restoreAll();
    if (sEmtrMoved) {
        Acc::trans(e) = sEmtrTrans;
        MTXCopy(sEmtrRot, Acc::rot(e));
        sEmtrMoved = false;
    }
    if (sEmtrTicked) {
        Acc::setTick(e, sEmtrTick);
        sEmtrTicked = false;
    }
    sEmtr = nullptr;
    sBlendNs += nowNs() - t0;
}

void pc_fps60_particles_emitter_turn(JPABaseEmitter* e, float v[3]) {
    if (sEmtr != e || !sEmtrMoved) {
        return;
    }
    const f32 x = v[0], y = v[1], z = v[2];
    for (int r = 0; r < 3; r++) {
        v[r] = sEmtrTurn[r][0] * x + sEmtrTurn[r][1] * y + sEmtrTurn[r][2] * z;
    }
}

float pc_fps60_particles_strip_shift(JPABaseEmitter* e) {
    if (sEmtr != e || !sEmtrMoved) {
        return 0.0f;
    }
    return (1.0f - sT) * (float)sBornActive;
}

void pc_fps60_particles_stats(char* out, unsigned long size, double frames) {
    if (frames <= 0) {
        frames = 1;
    }
    const double paints = sPaints != 0 ? (double)sPaints : 1.0;
    snprintf(out, size,
             ", particles: %.0f captured, %.0f blended %.0f born-moved %.0f held (%.0f emitters) per paint B, "
             "capture %.3f ms, blend %.3f ms",
             sCaptured / frames, sBlended / paints, sBorn / paints, sHeld / paints, sEmitters / paints,
             sCaptureNs / frames / 1e6, sBlendNs / frames / 1e6);
    sCaptureNs = sBlendNs = 0;
    sCaptured = sBlended = sBorn = sHeld = sEmitters = sPaints = 0;
}
