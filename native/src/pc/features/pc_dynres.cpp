// Dynamic resolution of the 3D scene (native/include/pc/pc_dynres.h; docs/SWITCH_PERF_STUDY.md,
// section 8): the painter renders the 3D part of a frame into the top-left part of the EFB through
// Aurora's content scale (native/patches/aurora/0009), copies that part and draws it stretched
// over the whole EFB, then the 2D at the full resolution. The level follows the GPU timer.
#include "pc/pc_dynres.h"

#include "pc_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include <dolphin/gx.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/mtx.h>

#include "JSystem/JKernel/JKRHeap.h"

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

namespace {

enum class Mode { Off, Auto, Fixed };

struct State {
    bool init = false;
    Mode mode = Mode::Off;
    float base = 0.f;                // COS_FB_SCALE
    std::vector<float> levels;       // levels[0] = base, then lower scales
    size_t level = 0;
    unsigned int cycle = 0;          // COS_DYNRES_CYCLE
    double highMs = 30.0;
    double lowMs = 27.0;
    std::vector<double> gpuMs;       // the last 60 frames' GPU times
    size_t gpuPos = 0;
    unsigned int samples = 0;        // samples since the last level change
    unsigned int sinceEval = 0;
    unsigned int overCount = 0;
    unsigned int underCount = 0;
    uint64_t lastGpuFrames = 0;
    uint64_t lastGpuNs = 0;
    bool applied = false;            // the 3D of this frame runs at a reduced scale
    float appliedScale = 1.f;
} s;

double envDouble(const char* name, double fallback) {
    const char* v = getenv(name);
    if (v == nullptr || *v == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const double d = strtod(v, &end);
    return end != v ? d : fallback;
}

// v: the COS_DYNRES value; base: the COS_FB_SCALE value (0 when unset).
void initWith(const char* v, float base) {
    s.init = true;
    if (v == nullptr || *v == '\0' || strcmp(v, "0") == 0) {
        return;
    }
    s.base = base;
    if (!(s.base > 0.f)) {
        pc::writef(STDERR_FILENO, "[cos] COS_DYNRES needs COS_FB_SCALE (a fixed internal resolution); off\n");
        return;
    }
    s.levels.push_back(s.base);
    if (strncmp(v, "fixed:", 6) == 0) {
        const float fixed = strtof(v + 6, nullptr);
        if (!(fixed > 0.f) || fixed >= s.base) {
            pc::writef(STDERR_FILENO, "[cos] COS_DYNRES=%s: the scale must be below COS_FB_SCALE=%.3f; off\n", v,
                       s.base);
            return;
        }
        s.levels.push_back(fixed);
        s.level = 1;
        s.mode = Mode::Fixed;
    } else {
        const char* list = getenv("COS_DYNRES_LEVELS");
        if (list == nullptr || *list == '\0') {
            list = "1.25,1.125";
        }
        while (*list != '\0') {
            char* end = nullptr;
            const float l = strtof(list, &end);
            if (end == list) {
                break;
            }
            if (l > 0.f && l < s.levels.back()) {
                s.levels.push_back(l);
            }
            list = *end == ',' ? end + 1 : end;
        }
        s.mode = Mode::Auto;
        s.cycle = (unsigned int)envDouble("COS_DYNRES_CYCLE", 0.0);
        s.highMs = envDouble("COS_DYNRES_HIGH", 30.0);
        s.lowMs = envDouble("COS_DYNRES_LOW", 27.0);
    }
    s.gpuMs.assign(60, 0.0);
    std::string levels;
    for (float l : s.levels) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s%.3f", levels.empty() ? "" : " ", l);
        levels += buf;
    }
    pc::writef(STDERR_FILENO,
               "[cos] dynres: %s, levels %s (3D only; 2D at the full scale)%s; down above %.1f ms GPU p95, "
               "up under %.1f ms predicted\n",
               s.mode == Mode::Fixed ? "fixed" : "auto", levels.c_str(),
#if defined(__SWITCH__)
               "",
#else
               s.mode == Mode::Auto && s.cycle == 0 ? " (no GPU timer here: stays at the base)" : "",
#endif
               s.highMs, s.lowMs);
}

void init() {
    initWith(getenv("COS_DYNRES"), (float)envDouble("COS_FB_SCALE", 0.0));
}

double p95() {
    const size_t n = std::min<size_t>(s.samples, s.gpuMs.size());
    if (n == 0) {
        return 0.0;
    }
    std::vector<double> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        v.push_back(s.gpuMs[(s.gpuPos + s.gpuMs.size() - 1 - i) % s.gpuMs.size()]);
    }
    std::sort(v.begin(), v.end());
    return v[(n - 1) * 95 / 100];
}

void setLevel(size_t level, unsigned int frame, double gpuP95, const char* why) {
    if (level == s.level || level >= s.levels.size()) {
        return;
    }
    pc::writef(STDERR_FILENO, "[cos] dynres: frame %u, %.3f -> %.3f (%s, GPU p95 %.2f ms)\n", frame,
               s.levels[s.level], s.levels[level], why, gpuP95);
    s.level = level;
    s.samples = 0;
    s.sinceEval = 0;
    s.overCount = 0;
    s.underCount = 0;
}

// A full-viewport quad textured with the RGBA8 copy `buf`, bilinear, replacing colour and alpha.
void drawStretched(void* buf, u16 w, u16 h) {
    GXTexObj texObj;
    GXInitTexObj(&texObj, buf, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetFog(GX_FOG_NONE, 0.0f, 1.0f, 0.1f, 1.0f, GXColor{0, 0, 0, 0});
    GXSetFogRangeAdj(GX_FALSE, 0, nullptr);
    GXSetCullMode(GX_CULL_NONE);
    Mtx44 ortho;
    C_MTXOrtho(ortho, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 10.0f);
    GXSetProjection(ortho, GX_ORTHOGRAPHIC);
    Mtx identity;
    PSMTXIdentity(identity);
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3s8(0, 0, -5);
    GXTexCoord2s8(0, 0);
    GXPosition3s8(1, 0, -5);
    GXTexCoord2s8(1, 0);
    GXPosition3s8(1, 1, -5);
    GXTexCoord2s8(1, 1);
    GXPosition3s8(0, 1, -5);
    GXTexCoord2s8(0, 1);
    GXEnd();
}

} // namespace

extern "C" {

void pc_dynres_frame_begin(unsigned int frame) {
    if (!s.init) {
        init();
    }
    if (s.mode != Mode::Auto) {
        return;
    }
    if (s.cycle != 0) {
        if (frame % s.cycle == 0) {
            setLevel((s.level + 1) % s.levels.size(), frame, 0.0, "COS_DYNRES_CYCLE");
        }
        return;
    }
#if defined(__SWITCH__)
    // GPU frames read back and their GPU time: Dawn GL's timer or the deko3d timestamps
    // (cos_switch_stats.cpp fills the same fields for both NROs)
    CosSwitchGfxStats g{};
    cos_switch_gfx_stats(&g);
    const uint64_t frames = g.gpuFrames, ns = g.gpuTotalNs;
    if (frames > s.lastGpuFrames && s.lastGpuFrames != 0) {
        const double ms = (double)(ns - s.lastGpuNs) / 1e6 / (double)(frames - s.lastGpuFrames);
        s.gpuMs[s.gpuPos] = ms;
        s.gpuPos = (s.gpuPos + 1) % s.gpuMs.size();
        s.samples++;
        s.sinceEval++;
    }
    s.lastGpuFrames = frames;
    s.lastGpuNs = ns;
#endif
    // Evaluate every 30 GPU frames once 60 frames at this level are in.
    if (s.samples < s.gpuMs.size() || s.sinceEval < 30) {
        return;
    }
    s.sinceEval = 0;
    const double p = p95();
    if (p > s.highMs) {
        s.underCount = 0;
        if (++s.overCount >= 2 && s.level + 1 < s.levels.size()) {
            setLevel(s.level + 1, frame, p, "GPU-bound");
        }
    } else {
        s.overCount = 0;
        if (s.level > 0) {
            const double ratio = (double)s.levels[s.level - 1] / (double)s.levels[s.level];
            const double predicted = p * ratio * ratio;
            if (predicted < s.lowMs) {
                if (++s.underCount >= 4) {
                    setLevel(s.level - 1, frame, p, "headroom");
                }
            } else {
                s.underCount = 0;
            }
        }
    }
}

void pc_dynres_configure(const char* mode, float base) {
    if (s.applied) {
        AuroraSetContentScale(1.0f);
    }
    s = State{};
    initWith(mode, base);
    if (s.mode == Mode::Off) {
        pc::writef(STDERR_FILENO, "[cos] dynres: off (options menu)\n");
    }
}

float pc_dynres_content_scale(void) { return s.applied ? s.appliedScale : 1.f; }

void pc_dynres_3d_begin(void) {
    if (!s.init) {
        init();
    }
    s.applied = false;
    if (s.mode == Mode::Off || s.level == 0) {
        return;
    }
    s.appliedScale = s.levels[s.level] / s.base;
    s.applied = true;
    AuroraSetContentScale(s.appliedScale);
}

void pc_dynres_3d_end(void) {
    if (!s.applied) {
        return;
    }
    static std::vector<unsigned char> sBuffer;
    if (sBuffer.empty()) {
        JKRPcHostAllocScope hostAlloc; // not the game's heaps
        sBuffer.resize(2560 * 1440 * 4);
    }
    // The 3D part (logical 640x480 maps to it while the scale is set) into a copy texture of its
    // own pixel size, then the full EFB again and the copy stretched over it.
    GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
    GXSetScissor(0, 0, 640, 480);
    GXSetTexCopySrc(0, 0, 640, 480);
    GXSetTexCopyDst(640, 480, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(sBuffer.data(), GX_FALSE);
    GXPixModeSync();
    AuroraSetContentScale(1.0f);
    s.applied = false;
    GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
    GXSetScissor(0, 0, 640, 480);
    drawStretched(sBuffer.data(), 640, 480);
}

} // extern "C"
