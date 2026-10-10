// COS_FPS60_TEST step D (docs/FPS60_PLAN.md, "Step D"): the smaller paint-B interpolations.
//
// - Rope / line packets (mDoExt_3DlineMat0_c, mDoExt_3DlineMat1_c: bridges, rafts, the grappling hook,
//   the boat's salvage rope, lifts, Puppet Ganon's strings, ...): update() builds each line's ribbon
//   in the draw pass into one of two arrays (mPosArr[mCurArr]) and the paint that draws it flips
//   mCurArr (paint B does not, step A), so in paint B the array the draw pass just filled is current
//   and the other one still holds what paint A drew. pc_fps60_line_positions blends the two into a
//   scratch array (paint A must have drawn the line with the same segment count in this game frame;
//   no point moved more than 300 units) that the GX array then points at. GXSetArray records the
//   address in the GX stream and Aurora's FIFO worker reads it when it translates the draw, maybe
//   after paint B returned: the scratch memory rotates over three buffers, one per paint B, reused
//   two paints B later (the reasoning of the models' arenas, pc_fps60_models.cpp).
// - Motion blur (motionBlure, m_Do_graphic.cpp): each present blends the previous present's colour
//   copy at the game's rate a (0-255). At two presents a game frame the trail would fade twice as
//   fast (a^2 per 1/30 s) and the blur matrix (a zoom/turn per present) act twice; a present that
//   follows another by one retrace (paint B; paint A after a frame that ended with paint B) uses
//   255 * sqrt(a / 255) and the matrix halfway to the identity.
//
// COS_FPS60_LINES=0 and COS_FPS60_BLUR=0 turn them off.
#include "JSystem/JSystem.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "pc_internal.h"

#include "dolphin/mtx/mtx.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace {

bool envOn(const char* name) {
    const char* v = getenv(name);
    return !(v != nullptr && v[0] == '0');
}

uint64_t nowNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

constexpr f32 kMaxMove = 300.0f;

// ---- scratch memory for arrays the GX stream points at (three buffers, one per paint B) ----
struct Scratch {
    unsigned char* data = nullptr;
    size_t cap = 0, used = 0;
};
Scratch sScratch[3];
unsigned int sScratchIdx = 0;
unsigned int sScratchFrame = 0; // the paint B (frame) the current buffer belongs to

void* scratchAlloc(size_t bytes) {
    const unsigned int frame = pc_frame_count() + 1;
    if (frame != sScratchFrame) {
        // a new paint B: the next buffer (its paint B was two paints B ago and has been processed)
        sScratchFrame = frame;
        sScratchIdx = (sScratchIdx + 1) % 3;
        sScratch[sScratchIdx].used = 0;
    }
    Scratch& s = sScratch[sScratchIdx];
    bytes = (bytes + 31) & ~(size_t)31;
    if (s.used + bytes > s.cap) {
        if (s.used != 0) {
            return nullptr; // this paint's earlier arrays are in the stream already: no realloc now
        }
        size_t n = s.cap != 0 ? s.cap : 64 * 1024;
        while (n < bytes) {
            n *= 2;
        }
        void* q = realloc(s.data, n);
        if (q == nullptr) {
            return nullptr;
        }
        s.data = (unsigned char*)q;
        s.cap = n;
    }
    void* p = s.data + s.used;
    s.used += bytes;
    return p;
}

// ---- lines: what paint A drew, by line packet ----
struct LineRec {
    const void* key;
    unsigned int frame;
    unsigned int count; // positions drawn per line
};
constexpr int kLineRecMax = 128;
LineRec sLineRecs[kLineRecMax];
int sNLineRecs = 0;

uint64_t sLineNs = 0;
unsigned long sLinesBlended = 0, sLinesHeld = 0, sLinePaints = 0;
unsigned int sLineStatFrame = 0;

LineRec* findLine(const void* key) {
    for (int i = 0; i < sNLineRecs; i++) {
        if (sLineRecs[i].key == key) {
            return &sLineRecs[i];
        }
    }
    return nullptr;
}

} // namespace

const float* pc_fps60_line_positions(const void* key, int lineNo, const float* cur, const float* prev,
                                     unsigned int count) {
    static const bool on = envOn("COS_FPS60_LINES");
    if (!on || !pc_fps60_test()) {
        return cur;
    }
    const unsigned int frame = pc_frame_count();
    if (!pc_paint_is_extra()) {
        // paint A: remember that this packet drew `count` positions per line in this game frame
        if (lineNo != 0) {
            return cur;
        }
        LineRec* r = findLine(key);
        if (r == nullptr) {
            if (sNLineRecs == kLineRecMax) {
                memmove(&sLineRecs[0], &sLineRecs[1], sizeof(LineRec) * (kLineRecMax - 1));
                sNLineRecs--;
            }
            r = &sLineRecs[sNLineRecs++];
            r->key = key;
        }
        r->frame = frame;
        r->count = count;
        return cur;
    }
    if (!pc_fps60_paint_blending()) {
        return cur;
    }
    const float t = pc_fps60_paint_t();
    const uint64_t t0 = nowNs();
    if (sLineStatFrame != frame) {
        sLineStatFrame = frame;
        sLinePaints++;
    }
    const LineRec* r = findLine(key);
    if (r == nullptr || r->frame != frame || r->count != count) {
        sLinesHeld++;
        sLineNs += nowNs() - t0;
        return cur;
    }
    for (unsigned int i = 0; i < count; i++) {
        const float dx = cur[i * 3] - prev[i * 3], dy = cur[i * 3 + 1] - prev[i * 3 + 1],
                    dz = cur[i * 3 + 2] - prev[i * 3 + 2];
        if (dx * dx + dy * dy + dz * dz > kMaxMove * kMaxMove) {
            sLinesHeld++;
            sLineNs += nowNs() - t0;
            return cur;
        }
    }
    float* out = (float*)scratchAlloc((size_t)count * 3 * sizeof(float));
    if (out == nullptr) {
        sLinesHeld++;
        sLineNs += nowNs() - t0;
        return cur;
    }
    for (unsigned int i = 0; i < count * 3; i++) {
        out[i] = prev[i] + (cur[i] - prev[i]) * t;
    }
    sLinesBlended++;
    sLineNs += nowNs() - t0;
    return out;
}

unsigned char pc_fps60_blur_rate(unsigned char rate, const float m[3][4], float out[3][4]) {
    MTXCopy(m, out);
    static const bool on = envOn("COS_FPS60_BLUR");
    if (!on || !pc_fps60_presents_close()) {
        return rate;
    }
    // the same decay per 1/30 s with two presents: sqrt per present; the matrix's step halved
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            const float id = r == c ? 1.0f : 0.0f;
            out[r][c] = id + (m[r][c] - id) * 0.5f;
        }
    }
    return (unsigned char)(255.0f * std::sqrt(rate / 255.0f) + 0.5f);
}

void pc_fps60_misc_stats(char* out, unsigned long size, double frames) {
    if (frames <= 0) {
        frames = 1;
    }
    const double paints = sLinePaints != 0 ? (double)sLinePaints : 1.0;
    snprintf(out, size, ", lines: %.1f blended %.1f held per paint B, %.3f ms", sLinesBlended / paints,
             sLinesHeld / paints, sLineNs / frames / 1e6);
    sLineNs = 0;
    sLinesBlended = sLinesHeld = sLinePaints = 0;
}
