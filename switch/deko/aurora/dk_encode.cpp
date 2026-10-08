// Aurora's recorded frame to deko3d commands (dk_gfx.hpp; docs/DEKO3D_MIGRATION_PLAN.md phase 3), on
// the render worker: the work encoding.cpp gives Dawn, op by op as Aurora seals them.
//
// Frame data. Aurora's staging slot is one deko3d block (vertices 5, uniforms 24, indices 2, storage
// 8, texture uploads 24 MiB at encoding.cpp's offsets) that the FIFO thread writes and the GPU reads
// in place: storage buffers 0 (vbuf) and 1 (abuf) are those regions with their sizes (the shaders'
// bounds checks use length()), uniform buffer 0 is the uniform region at each draw's dynamic offset
// (3840 bytes, both stages), the index buffer the index region at the draw's offset; the 64-byte
// immediates go to uniform buffer 15 (both stages: 456 fragment shaders read them) by
// dkCmdBufPushConstants. The slot is reused once the GPU has finished its frame (staging_ready).
//
// Passes. Render targets bound from the views' shadows, load-op clears with deko3d's clear commands,
// then the pass's commands: viewport (reversed Z: depth range 1 - far .. 1 - near, as encoding.cpp),
// scissor (clamped to the target), GX draws, partial-clear draws. Every pass ends with a Fragments
// barrier that also invalidates the texture cache (plan section 7.6: one per pass end for now), and
// is followed by its EFB copy (the copy engine for a same-format copy, else a conversion or blit draw
// with the fixed shaders), its snapshots and, before it, its palette conversions. Copy-engine work
// (uploads, copies) sits between Full barriers.
//
// State cache. Pipeline state, shaders, viewport, scissor, blend constant, uniform offset, texture
// handles, index buffer and immediates are bound only when they change within a frame; anything that
// binds its own state (fixed passes, the present) resets it.
//
// The command list goes to the GPU every COS_DK_SUBMIT_DRAWS draws (default 256, 0 = once per
// frame), as SwitchWakerHD's, so the GPU starts on a frame while the FIFO thread still records it.
#include "dk_encode.hpp"

#include "dk_gfx.hpp"

#include "gfx/clear.hpp"
#include "gfx/frame.hpp"
#include "gfx/pipeline_cache.hpp"
#include "gfx/resource_cache.hpp"
#include "gfx/tex_copy_conv.hpp"
#include "gfx/tex_palette_conv.hpp"
#include "gfx/texture.hpp"
#include "gx/gx.hpp"
#include "gx/pipeline.hpp"
#include "webgpu/gpu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace aurora::gfx::dk {
using namespace aurora::gfx::detail;
using swdk::dklog;
using swdk::R;

namespace {

// Aurora's staging layout (encoding.cpp)
constexpr uint64_t kVertexOffset = 0;
constexpr uint64_t kUniformOffset = kVertexOffset + VertexBufferSize;
constexpr uint64_t kIndexOffset = kUniformOffset + UniformBufferSize;
constexpr uint64_t kStorageOffset = kIndexOffset + IndexBufferSize;
constexpr uint64_t kTextureUploadOffset = kStorageOffset + StorageBufferSize;
constexpr uint32_t kImmediatesSize = 256;  // the uniform buffer 15 region (64 bytes used)

struct Staging {
    swdk::Block block;
    std::atomic<uint64_t> frame{0};  // the last frame that read it
};
Staging g_staging[StagingBufferCount];

struct FrameState {
    bool open = false;
    DkGpuAddr staging = 0;
    size_t uploadsDone = 0;
    DkGpuAddr immediates = 0;
    uint32_t drawsSinceSubmit = 0;
    bool workSinceTransfer = false;
} g_frame;

struct Cache {
    bool globals = false;
    const Pipe* pipe = nullptr;
    const DkShader* vs = nullptr;
    const DkShader* fs = nullptr;
    bool rsValid = false, csValid = false, cwValid = false, bsValid = false, dsValid = false, biasValid = false;
    DkRasterizerState rs{};
    DkColorState cs{};
    DkColorWriteState cw{};
    DkBlendState bs{};
    DkDepthStencilState ds{};
    float bias[3] = {};
    bool vpValid = false, scValid = false, blendValid = false, immValid = false;
    DkViewport vp{};
    DkScissor sc{};
    float blend[4] = {};
    uint8_t imm[64] = {};
    WGPUBindGroup textures = nullptr;
    DkGpuAddr ubo = 0;
    DkGpuAddr idx = 0;
} g_cache;

EncodeStats g_stats;
uint32_t g_submitDraws = 256;

// ---- GPU timestamps: every frame, at its start, the end of every pass and copy, the present and
// ImGui; read when the frame's slot comes round again (its fence waited for)
constexpr uint32_t kMaxMarks = swdk::kQuerySize / swdk::kFrames / 16;
struct Marks {
    GpuWork work[kMaxMarks];
    uint32_t count = 0;
    uint64_t cpuNs = 0;  // when the frame was opened
};
Marks g_marks[swdk::kFrames];
bool g_gpuTimers = true;

// the GPU's timer against the CPU's (SwitchWakerHD backend.cpp GpuClock): deko3d's dkTimestampToNs
// assumes 31.25 MHz, the Tegra X1's runs at 19.2 MHz; measured over the session from the frames' first
// timestamps and the CPU time of their start
struct GpuClock {
    bool have = false, logged = false;
    uint64_t cpu0 = 0, gpu0 = 0;
    double factor = 31.25 / 19.2;
    void sample(uint64_t cpuNs, uint64_t gpuNs) {
        if (!have) {
            have = true;
            cpu0 = cpuNs;
            gpu0 = gpuNs;
            return;
        }
        if (gpuNs <= gpu0 || cpuNs - cpu0 < 4'000'000'000ull) return;
        factor = double(cpuNs - cpu0) / double(gpuNs - gpu0);
        if (!logged && cpuNs - cpu0 > 30'000'000'000ull) {
            logged = true;
            dklog("GPU timer: %.4f real ns per deko3d ns (measured over %.0f s); the GPU times are real time", factor,
                  double(cpuNs - cpu0) / 1e9);
        }
    }
} g_clock;

void read_marks(uint64_t frame) {
    Marks& m = g_marks[frame % swdk::kFrames];
    if (m.count >= 2) {
        const swdk::QueryRegion q = swdk::query_region(frame);
        uint64_t ts[kMaxMarks];
        for (uint32_t i = 0; i < m.count; i++) memcpy(&ts[i], q.cpu + size_t(i) * 16 + 8, 8);
        g_clock.sample(m.cpuNs, dkTimestampToNs(ts[0]));
        bool ok = true;
        for (uint32_t i = 1; i < m.count && ok; i++) ok = ts[i] >= ts[i - 1];
        if (ok) {
            for (uint32_t i = 1; i < m.count; i++)
                g_stats.gpuNs[size_t(m.work[i])] += uint64_t(double(dkTimestampToNs(ts[i] - ts[i - 1])) * g_clock.factor);
            g_stats.gpuTotalNs += uint64_t(double(dkTimestampToNs(ts[m.count - 1] - ts[0])) * g_clock.factor);
            g_stats.gpuFrames++;
        } else {
            g_stats.gpuDropped++;
        }
    }
    m.count = 0;
}

void barrier(DkBarrier mode, uint32_t invalidate) {
    dkCmdBufBarrier(R.cmd, mode, invalidate);
    g_stats.barriers++;
}

void transfer_begin() {
    if (g_frame.workSinceTransfer) barrier(DkBarrier_Full, DkInvalidateFlags_Image);
    g_frame.workSinceTransfer = false;
}
void transfer_end() { barrier(DkBarrier_Full, DkInvalidateFlags_Image); }

void set_swizzle(bool flip) {
    const DkViewportSwizzle sw = {DkSwizzle_PositiveX, flip ? DkSwizzle_NegativeY : DkSwizzle_PositiveY,
                                  DkSwizzle_PositiveZ, DkSwizzle_PositiveW};
    dkCmdBufSetViewportSwizzles(R.cmd, 0, &sw, 1);
}

// the frame-wide bindings of GX draws
void ensure_globals() {
    if (g_cache.globals) return;
    const DkGpuAddr s = g_frame.staging;
    dkCmdBufBindStorageBuffer(R.cmd, DkStage_Vertex, 0, s + kVertexOffset, uint32_t(VertexBufferSize));
    dkCmdBufBindStorageBuffer(R.cmd, DkStage_Vertex, 1, s + kStorageOffset, uint32_t(StorageBufferSize));
    dkCmdBufBindStorageBuffer(R.cmd, DkStage_Fragment, 1, s + kStorageOffset, uint32_t(StorageBufferSize));
    dkCmdBufBindUniformBuffer(R.cmd, DkStage_Vertex, swdk::kImmediatesUboSlot, g_frame.immediates, kImmediatesSize);
    dkCmdBufBindUniformBuffer(R.cmd, DkStage_Fragment, swdk::kImmediatesUboSlot, g_frame.immediates, kImmediatesSize);
    dkCmdBufBindVtxAttribState(R.cmd, nullptr, 0);
    dkCmdBufBindVtxBufferState(R.cmd, nullptr, 0);
    set_swizzle(swdk::C.flipY);
    g_cache.globals = true;
}

template <class T> bool changed(bool& valid, T& cached, const T& now) {
    if (valid && memcmp(&cached, &now, sizeof now) == 0) return false;
    cached = now;
    valid = true;
    return true;
}

void bind_pipe(const Pipe* p) {
    if (g_cache.pipe == p) return;
    const DkShader* vs = &p->vs->shader;
    const DkShader* fs = &p->fs->shader;
    if (vs != g_cache.vs || fs != g_cache.fs) {
        if (swdk::code_take_written()) dkCmdBufBarrier(R.cmd, DkBarrier_None, DkInvalidateFlags_Shader);
        const DkShader* sh[] = {vs, fs};
        dkCmdBufBindShaders(R.cmd, DkStageFlag_GraphicsMask, sh, 2);
        g_cache.vs = vs;
        g_cache.fs = fs;
        g_stats.shaderBinds++;
    }
    if (changed(g_cache.rsValid, g_cache.rs, p->rasterizer)) dkCmdBufBindRasterizerState(R.cmd, &p->rasterizer);
    if (changed(g_cache.csValid, g_cache.cs, p->color)) dkCmdBufBindColorState(R.cmd, &p->color);
    if (changed(g_cache.cwValid, g_cache.cw, p->colorWrite)) dkCmdBufBindColorWriteState(R.cmd, &p->colorWrite);
    if (changed(g_cache.bsValid, g_cache.bs, p->blend)) dkCmdBufBindBlendStates(R.cmd, 0, &p->blend, 1);
    if (changed(g_cache.dsValid, g_cache.ds, p->depthStencil)) dkCmdBufBindDepthStencilState(R.cmd, &p->depthStencil);
    if (p->rasterizer.depthBiasEnableMask) {
        float bias[3];
        memcpy(bias, p->depthBias, sizeof bias);
        if (!g_cache.biasValid || memcmp(g_cache.bias, bias, sizeof bias) != 0) {
            dkCmdBufSetDepthBias(R.cmd, bias[0], bias[1], bias[2]);
            memcpy(g_cache.bias, bias, sizeof bias);
            g_cache.biasValid = true;
        }
    }
    g_cache.pipe = p;
    g_stats.pipelineBinds++;
}

void set_blend_const(float r, float g, float b, float a) {
    const float c[4] = {r, g, b, a};
    if (g_cache.blendValid && memcmp(g_cache.blend, c, sizeof c) == 0) return;
    dkCmdBufSetBlendConst(R.cmd, r, g, b, a);
    memcpy(g_cache.blend, c, sizeof c);
    g_cache.blendValid = true;
}

void apply_viewport(const Viewport& vp) {
    if (!(vp.width > 0.f) || !(vp.height > 0.f)) return;  // (deko3d's viewport size is unsigned)
    const float minDepth = gx::UseReversedZ ? 1.f - vp.zfar : vp.znear;
    const float maxDepth = gx::UseReversedZ ? 1.f - vp.znear : vp.zfar;
    const DkViewport v = {vp.left, vp.top, vp.width, vp.height, minDepth, maxDepth};
    if (changed(g_cache.vpValid, g_cache.vp, v)) dkCmdBufSetViewports(R.cmd, 0, &v, 1);
}

void apply_scissor(const ClipRect& sc, uint32_t tw, uint32_t th) {
    const auto x = std::clamp(static_cast<uint32_t>(std::max(sc.x, 0)), 0u, tw);
    const auto y = std::clamp(static_cast<uint32_t>(std::max(sc.y, 0)), 0u, th);
    const auto w = std::clamp(static_cast<uint32_t>(std::max(sc.width, 0)), 0u, tw - x);
    const auto h = std::clamp(static_cast<uint32_t>(std::max(sc.height, 0)), 0u, th - y);
    const DkScissor s = {x, y, w, h};
    if (changed(g_cache.scValid, g_cache.sc, s)) dkCmdBufSetScissors(R.cmd, 0, &s, 1);
}

void maybe_submit() {
    if (g_submitDraws == 0 || ++g_frame.drawsSinceSubmit < g_submitDraws) return;
    g_frame.drawsSinceSubmit = 0;
    swdk::submit("draws");
    g_stats.submits++;
}

void log_once(const char* what) {
    static std::vector<std::string> seen;
    for (auto& s : seen)
        if (s == what) return;
    seen.emplace_back(what);
    dklog("encoder: %s (not supported by the deko3d renderer yet; logged once)", what);
}

Tex* g_passDepth = nullptr;  // the current pass's depth target (COS_DK_DUMP_DEPTH)
Tex* g_passColor = nullptr;  // its colour attachment 0 (COS_DK_DUMP_COLOR)

// COS_DK_DUMP_COLOR=<module hex> (diagnostic): before the first draw of that module (from COS_DK_TRACE_FRAME on),
// the pass's colour attachment 0 is written raw (RGBA8) to native/trace_color_<w>x<h>.raw
void dump_color(uint64_t module) {
    static const uint64_t want = [] {
        const char* e = getenv("COS_DK_DUMP_COLOR");
        return e && *e ? strtoull(e, nullptr, 16) : 0ull;
    }();
    static const long minFrame = swdk::env_long("COS_DK_TRACE_FRAME", 0);
    // every draw of the module in the first frame that has one (from COS_DK_TRACE_FRAME on), up to 16 files
    static long seen = 0;
    static uint64_t frame = 0;
    if (!want || module != want || !g_passColor || R.frame < uint64_t(std::max(minFrame, 0L))) return;
    if (frame == 0) frame = R.frame;
    if (R.frame != frame || seen >= 16) return;
    const long nth = ++seen;
    // what the pass drew so far: finished and out of the render target caches before the copy reads it
    dkCmdBufBarrier(R.cmd, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_L2Cache);
    std::vector<uint8_t> px;
    uint32_t w = 0, h = 0;
    wgpu::TextureFormat fmt{};
    if (!read_texture(g_passColor->handle, px, w, h, fmt)) return;
    char path[160];
    snprintf(path, sizeof path, "/switch/switchwaker/native/trace_color_%ux%u_%02ld.raw", w, h, nth);
    if (FILE* f = fopen(path, "wb")) {
        fwrite(px.data(), 1, px.size(), f);
        fclose(f);
        dklog("trace: colour target (%s) written to %s", g_passColor->label.c_str(), path);
    }
}

// COS_DK_DUMP_DEPTH=<module hex> (diagnostic): before the first draw of that module, the pass's depth buffer is
// written raw to native/trace_depth_<w>x<h>_<fmt>.raw
void dump_depth(uint64_t module) {
    static const uint64_t want = [] {
        const char* e = getenv("COS_DK_DUMP_DEPTH");
        return e && *e ? strtoull(e, nullptr, 16) : 0ull;
    }();
    static bool done = false;
    static const long minFrame = swdk::env_long("COS_DK_TRACE_FRAME", 0);  // (not before that frame)
    if (!want || done || module != want || !g_passDepth || R.frame < uint64_t(std::max(minFrame, 0L))) return;
    done = true;
    std::vector<uint8_t> px;
    uint32_t w = 0, h = 0;
    wgpu::TextureFormat fmt{};
    if (!read_texture(g_passDepth->handle, px, w, h, fmt)) return;
    char path[160];
    snprintf(path, sizeof path, "/switch/switchwaker/native/trace_depth_%ux%u_%d.raw", w, h, int(g_passDepth->format));
    if (FILE* f = fopen(path, "wb")) {
        fwrite(px.data(), 1, px.size(), f);
        fclose(f);
        dklog("trace: depth buffer (%s) written to %s", g_passDepth->label.c_str(), path);
    }
}

// COS_DK_TRACE_FRAME=N (diagnostic): every GX draw of frame N logged, one line each (module, blend, the units with
// a real texture); COS_DK_SKIP_MODULE=<hex>[,<hex>...]: the draws of those modules are not drawn
bool trace_frame_now() {
    static const long n = swdk::env_long("COS_DK_TRACE_FRAME", 0);
    static const bool dumpOnly = (getenv("COS_DK_DUMP_DEPTH") && *getenv("COS_DK_DUMP_DEPTH")) ||
                                 (getenv("COS_DK_DUMP_COLOR") && *getenv("COS_DK_DUMP_COLOR"));
    return n > 0 && !dumpOnly && R.frame == uint64_t(n);
}
bool skip_module(uint64_t module) {
    static const std::vector<uint64_t> list = [] {
        std::vector<uint64_t> v;
        if (const char* e = getenv("COS_DK_SKIP_MODULE"); e && *e) {
            for (const char* p = e; *p;) {
                char* end = nullptr;
                v.push_back(strtoull(p, &end, 16));
                if (!end || end == p) break;
                p = *end == ',' ? end + 1 : end;
            }
            dklog("encoder: COS_DK_SKIP_MODULE: %zu modules not drawn", v.size());
        }
        return v;
    }();
    return std::find(list.begin(), list.end(), module) != list.end();
}
void trace_draw(const gx::DrawData& d, const Pipe* main, BindGroup* g) {
    static uint32_t index = 0;
    char units[256];
    size_t n = 0;
    units[0] = 0;
    if (g) {
        for (const auto& e : g->entries) {
            if (e.binding & 1 || e.binding / 2 >= 8 || !e.view || !e.view->tex) continue;
            const Tex* t = e.view->tex;
            if (t->width * t->height <= 1) continue;
            n += size_t(snprintf(units + n, sizeof units - n, " u%u:%ux%u%s", e.binding / 2, t->width, t->height,
                                 t->label == "Resolved Texture" ? "(copy)" : ""));
            if (n >= sizeof units) break;
        }
    }
    const DkBlendState& bl = main->blend;
    const auto& rs = main->rasterizer;
    dklog("trace frame %llu draw %u: module %016llx blend %d/%d/%d a %d/%d/%d mask 0x%x depth %d/%d op %d cull %d front %d "
          "bias 0x%x %.2f/%.2f/%.2f clamp %d %u idx %u vtx%s",
          (unsigned long long)R.frame, index++, (unsigned long long)main->moduleHash, int(bl.colorBlendOp),
          int(bl.srcColorBlendFactor), int(bl.dstColorBlendFactor), int(bl.alphaBlendOp), int(bl.srcAlphaBlendFactor),
          int(bl.dstAlphaBlendFactor), unsigned(main->colorWrite.masks & 0xF), int(main->depthStencil.depthTestEnable),
          int(main->depthStencil.depthWriteEnable), int(main->depthStencil.depthCompareOp), int(rs.cullMode),
          int(rs.frontFace), unsigned(rs.depthBiasEnableMask), main->depthBias[0], main->depthBias[1], main->depthBias[2],
          int(rs.depthClampEnable), d.indexCount, d.vtxCount, units);
}

// ---- draws
void draw_gx(const DrawCommand& cmd) {
    gx::DrawData d;
    memcpy(&d, cmd.payload.data(), sizeof d);
    CompiledPipeline cp;
    if (!get_pipeline(d.pipeline, cp)) {
        g_stats.skippedPipeline++;
        return;
    }
    const auto* main = static_cast<const Pipe*>(cp.dkMain);
    const auto* pre = static_cast<const Pipe*>(cp.dkPrepass);
    if (!pipe_ready(main) || (pre && !pipe_ready(pre))) {
        g_stats.skippedShader++;
        return;
    }
    if (trace_frame_now()) {
        WGPUBindGroup tg = d.bindGroups.textureBindGroup ? find_bind_group_raw(d.bindGroups.textureBindGroup) : nullptr;
        trace_draw(d, main, tg ? find_bind_group(tg) : nullptr);
    }
    if (skip_module(main->moduleHash)) return;
    dump_depth(main->moduleHash);
    dump_color(main->moduleHash);
    ensure_globals();
    if (!g_cache.immValid || memcmp(g_cache.imm, &d.immediateData, sizeof d.immediateData) != 0) {
        dkCmdBufPushConstants(R.cmd, g_frame.immediates, kImmediatesSize, 0, sizeof d.immediateData, &d.immediateData);
        memcpy(g_cache.imm, &d.immediateData, sizeof d.immediateData);
        g_cache.immValid = true;
    }
    const DkGpuAddr ubo = g_frame.staging + kUniformOffset + d.uniformRange.offset;
    if (ubo != g_cache.ubo) {
        dkCmdBufBindUniformBuffer(R.cmd, DkStage_Vertex, 0, ubo, gx::MaxUniformSize);
        dkCmdBufBindUniformBuffer(R.cmd, DkStage_Fragment, 0, ubo, gx::MaxUniformSize);
        g_cache.ubo = ubo;
        g_stats.uniformBinds++;
    }
    if (d.bindGroups.textureBindGroup) {
        // (without one the draw keeps the textures bound before, as in the WebGPU pass)
        WGPUBindGroup g = find_bind_group_raw(d.bindGroups.textureBindGroup);
        if (g && g != g_cache.textures) {
            if (BindGroup* sg = find_bind_group(g)) {
                dkCmdBufBindTextures(R.cmd, DkStage_Fragment, 0, gx_handles(sg), 8);
                g_stats.textureBinds++;
            }
            g_cache.textures = g;
        }
    }
    if (d.dstAlpha != UINT32_MAX) set_blend_const(0.f, 0.f, 0.f, float(d.dstAlpha) / 255.f);
    if (d.indexCount != 0) {
        const DkGpuAddr idx = g_frame.staging + kIndexOffset + d.idxRange.offset;
        if (idx != g_cache.idx) {
            dkCmdBufBindIdxBuffer(R.cmd, DkIdxFormat_Uint16, idx);
            g_cache.idx = idx;
        }
    }
    const auto draw = [&] {
        // first vertex and first instance 0: gl_VertexID / gl_InstanceID are WebGPU's indices then
        if (d.indexCount == 0) {
            dkCmdBufDraw(R.cmd, DkPrimitive_Triangles, d.vtxCount, d.instanceCount, 0, 0);
        } else {
            dkCmdBufDrawIndexed(R.cmd, DkPrimitive_Triangles, d.indexCount, d.instanceCount, 0, 0, 0);
        }
    };
    if (pre) {
        bind_pipe(pre);
        draw();
    }
    bind_pipe(main);
    draw();
    if (main->alphaPass) {
        bind_pipe(main->alphaPass);
        draw();
    }
    g_stats.draws++;
    g_frame.workSinceTransfer = true;
    maybe_submit();
}

void draw_clear(const DrawCommand& cmd, uint32_t tw, uint32_t th) {
    clear::DrawData d;
    memcpy(&d, cmd.payload.data(), sizeof d);
    CompiledPipeline cp;
    if (!get_pipeline(d.pipeline, cp)) {
        g_stats.skippedPipeline++;
        return;
    }
    const auto* p = static_cast<const Pipe*>(cp.dkMain);
    if (!pipe_ready(p)) {
        g_stats.skippedShader++;
        return;
    }
    ensure_globals();
    bind_pipe(p);
    set_blend_const(float(d.color.r), float(d.color.g), float(d.color.b), float(d.color.a));
    // clear.cpp: the whole target, depth range [depth, depth] (the value written), the clear's scissor
    const DkViewport v = {0.f, 0.f, float(tw), float(th), d.depth, d.depth};
    dkCmdBufSetViewports(R.cmd, 0, &v, 1);
    g_cache.vpValid = false;
    const DkScissor s = {uint32_t(std::max(d.rect.x, 0)), uint32_t(std::max(d.rect.y, 0)),
                         uint32_t(std::max(d.rect.width, 0)), uint32_t(std::max(d.rect.height, 0))};
    dkCmdBufSetScissors(R.cmd, 0, &s, 1);
    g_cache.scValid = false;
    dkCmdBufDraw(R.cmd, DkPrimitive_Triangles, 3, 1, 0, 0);
    g_stats.clears++;
    g_frame.workSinceTransfer = true;
}

// ---- uploads and copies (copy engine)
uint32_t level_size(uint32_t size, uint32_t level) { return std::max(size >> level, 1u); }

void upload(const TextureUpload& u) {
    Tex* t = find_texture(u.tex.texture.Get());
    if (!t || !t->valid) return;
    if (u.buffer && !u.dkGpu) {
        log_once("a texture upload from a WebGPU buffer");
        return;
    }
    const uint32_t mip = u.tex.mipLevel;
    if (mip >= t->mips) return;
    const DkGpuAddr src = u.dkGpu ? u.dkGpu : g_frame.staging + kTextureUploadOffset + u.layout.offset;
    const uint32_t rowBytes = (u.layout.bytesPerRow + 255u) & ~255u;
    const uint32_t rows = u.layout.rowsPerImage ? u.layout.rowsPerImage : 1;
    const DkCopyBuf buf = {src, rowBytes, rowBytes * rows};
    const DkImageView view = level_view(t, mip);
    const uint32_t w = std::min(u.size.width, level_size(t->width, mip) - std::min(u.tex.origin.x, level_size(t->width, mip)));
    const uint32_t h =
        std::min(u.size.height, level_size(t->height, mip) - std::min(u.tex.origin.y, level_size(t->height, mip)));
    if (w == 0 || h == 0) return;
    const DkImageRect rect = {u.tex.origin.x, u.tex.origin.y, u.tex.origin.z, w, h,
                              std::max<uint32_t>(u.size.depthOrArrayLayers, 1)};
    dkCmdBufCopyBufferToImage(R.cmd, &buf, &view, &rect, swdk::C.flipTexture ? uint32_t(DkBlitFlag_FlipY) : 0u);
    if (u.dkBlock) swdk::block_free_deferred(static_cast<DkMemBlock>(u.dkBlock));
    g_stats.uploads++;
    g_stats.uploadBytes += uint64_t(rowBytes) * rows;
}

void do_uploads(const FrameOp& op) {
    if (op.textureUploads.size() <= g_frame.uploadsDone) return;
    transfer_begin();
    for (size_t i = g_frame.uploadsDone; i < op.textureUploads.size(); i++) upload(*op.textureUploads[i]);
    g_frame.uploadsDone = op.textureUploads.size();
    transfer_end();
    gpu_mark(GpuWork::Copy);
}

void copy_image(Tex* src, uint32_t srcMip, uint32_t sx, uint32_t sy, Tex* dst, uint32_t dstMip, uint32_t dx,
                uint32_t dy, uint32_t w, uint32_t h) {
    if (!src || !dst || !src->valid || !dst->valid) return;
    if (src->format != dst->format && format_info(src->wformat).blockBytes != format_info(dst->wformat).blockBytes) {
        log_once("an image copy between formats of different sizes");
        return;
    }
    const uint32_t sw = level_size(src->width, srcMip), shh = level_size(src->height, srcMip);
    const uint32_t dw = level_size(dst->width, dstMip), dh = level_size(dst->height, dstMip);
    if (sx >= sw || sy >= shh || dx >= dw || dy >= dh) return;
    w = std::min({w, sw - sx, dw - dx});
    h = std::min({h, shh - sy, dh - dy});
    if (w == 0 || h == 0) return;
    if (src->compressed) {
        // dkCmdBufCopyImage takes compressed rectangles in blocks (SwitchWakerHD surfaces.cpp)
        w = (w + src->blockW - 1) / src->blockW;
        h = (h + src->blockH - 1) / src->blockH;
        sx /= src->blockW;
        sy /= src->blockH;
        dx /= dst->blockW;
        dy /= dst->blockH;
    }
    transfer_begin();
    const DkImageView sv = level_view(src, srcMip), dv = level_view(dst, dstMip);
    const DkImageRect sr = {sx, sy, 0, w, h, 1}, dr = {dx, dy, 0, w, h, 1};
    dkCmdBufCopyImage(R.cmd, &sv, &sr, &dv, &dr, 0);
    transfer_end();
    g_stats.copies++;
    gpu_mark(GpuWork::Copy);
}

// ---- conversions (draws with the fixed shaders)
const char* copy_conv_module(GXTexFmt fmt) {
    switch (fmt) {
    case GX_TF_I4: return "tex_copy_conv_i4";
    case GX_TF_I8: return "tex_copy_conv_i8";
    case GX_TF_IA4: return "tex_copy_conv_ia4";
    case GX_TF_IA8: return "tex_copy_conv_ia8";
    case GX_TF_RGB565: return "tex_copy_conv_rgb565";
    case GX_CTF_R4: return "tex_copy_conv_r4";
    case GX_CTF_RA4: return "tex_copy_conv_ra4";
    case GX_CTF_RA8: return "tex_copy_conv_ra8";
    case GX_CTF_A8: return "tex_copy_conv_a8";
    case GX_CTF_R8: return "tex_copy_conv_r8";
    case GX_CTF_G8: return "tex_copy_conv_g8";
    case GX_CTF_B8: return "tex_copy_conv_b8";
    case GX_CTF_RG8: return "tex_copy_conv_rg8";
    case GX_CTF_GB8: return "tex_copy_conv_gb8";
    case GX_TF_Z8: return "tex_copy_conv_z8";
    case GX_TF_Z16: return "tex_copy_conv_z16";
    default: return nullptr;
    }
}

DkImageView target_view(View* v) { return level_view(v->tex, v->baseMip); }

void convert_copy(GXTexFmt fmt, bool blit, const wgpu::TextureView& srcView, Range uniforms, const TextureHandle& dst,
                  bool linear) {
    const bool depth = !blit && gx::is_depth_format(fmt);
    const char* module = blit ? "tex_copy_conv_blit" : copy_conv_module(fmt);
    if (!module) {
        log_once("an EFB copy conversion of a format without a fixed shader");
        return;
    }
    View* src = find_view(srcView.Get());
    View* out = find_view(dst->attachmentTextureView.Get());
    if (!src || !out || !out->tex->valid) return;
    const DkImageView target = target_view(out);
    // tex_copy_conv's bindings: colour (sampler 0, texture 1, uniforms 2), depth (texture 0, uniforms 1)
    const FixedBind binds[] = {
        {swdk::SlotBinding::Kind::Uniform, depth ? 1u : 2u, 0, g_frame.staging + kUniformOffset + uniforms.offset,
         std::max<uint32_t>((uniforms.size + 15u) & ~15u, 16u)},
        {swdk::SlotBinding::Kind::Sampler, depth ? 0u : 1u,
         dkMakeTextureHandle(view_slot(src), linear ? swdk::kSamplerLinearClamp : swdk::kSamplerNearestClamp)},
    };
    if (fixed_draw(module, "vs_main", "fs_main", &target, out->tex->width, out->tex->height, true, binds, 2)) {
        g_stats.conversions++;
        gpu_mark(GpuWork::Conversion);
    }
}

void palette_conv(const tex_palette_conv::ConvRequest& req) {
    const char* module = req.variant == tex_palette_conv::Variant::Direct       ? "tex_palette_conv_direct"
                         : req.variant == tex_palette_conv::Variant::FromFloat8 ? "tex_palette_conv_fromfloat8"
                                                                                : "tex_palette_conv_fromfloat4";
    if (!req.src || !req.dst || !req.tlut) return;
    View* src = find_view(req.src->sampleTextureView.Get());
    View* tlut = find_view(req.tlut->sampleTextureView.Get());
    View* out = find_view(req.dst->attachmentTextureView.Get());
    if (!src || !tlut || !out || !out->tex->valid) return;
    const DkImageView target = target_view(out);
    const FixedBind binds[] = {
        {swdk::SlotBinding::Kind::Sampler, 1, dkMakeTextureHandle(view_slot(src), swdk::kSamplerNearestClamp)},
        {swdk::SlotBinding::Kind::Sampler, 2, dkMakeTextureHandle(view_slot(tlut), swdk::kSamplerNearestClamp)},
    };
    if (fixed_draw(module, "vs_main", "fs_main", &target, out->tex->width, out->tex->height, true, binds, 2)) {
        g_stats.conversions++;
        gpu_mark(GpuWork::Conversion);
    }
}

void depth_snapshot(const wgpu::TextureView& srcDepth, uint32_t samples, const wgpu::TextureView& dst) {
    View* src = find_view(srcDepth.Get());
    View* out = find_view(dst.Get());
    if (!src || !out || !out->tex->valid) return;
    const DkImageView target = target_view(out);
    const FixedBind binds[] = {
        {swdk::SlotBinding::Kind::Sampler, 0, dkMakeTextureHandle(view_slot(src), swdk::kSamplerNearestClamp)},
    };
    const char* module = samples > 1 ? "tex_copy_conv_depth_snapshot_ms" : "tex_copy_conv_depth_snapshot";
    if (fixed_draw(module, "vs_main", "fs_main", &target, out->tex->width, out->tex->height, true, binds, 1)) {
        g_stats.conversions++;
        gpu_mark(GpuWork::Conversion);
    }
}

// encoding.cpp render(): the EFB copy of a pass into its resolve target
void efb_copy(const RenderPass& pass) {
    const auto& dst = pass.resolveTarget;
    const auto& scene = pass.colorAttachments[SceneColorAttachmentIndex];
    const auto& rect = pass.resolveRect;
    const bool needsConversion = tex_copy_conv::needs_conversion(pass.resolveFormat);
    const bool needsScaling =
        dst->size.width != static_cast<uint32_t>(rect.width) || dst->size.height != static_cast<uint32_t>(rect.height);
    const bool isDepth = gx::is_depth_format(pass.resolveFormat);
    const bool needsOpaqueAlpha = !isDepth && !gx::efb_has_alpha(pass.resolveSourceFormat);
    const bool sameFormat = dst->format == scene.format;
    const auto& srcView = isDepth ? pass.copySourceDepthView : pass.copySourceView;
    if (needsConversion) {
        convert_copy(pass.resolveFormat, false, srcView, pass.resolveUniformRange, dst, needsScaling);
    } else if (needsScaling || needsOpaqueAlpha || !sameFormat) {
        convert_copy(pass.resolveFormat, true, srcView, pass.resolveUniformRange, dst, needsScaling);
    } else {
        copy_image(find_texture(pass.copySourceTexture.Get()), 0, uint32_t(std::max(rect.x, 0)),
                   uint32_t(std::max(rect.y, 0)), find_texture(dst->texture.Get()), 0, 0, 0, uint32_t(rect.width),
                   uint32_t(rect.height));
    }
}

// ---- passes
void encode_pass(RenderPass& pass) {
    if (!pass.sealed) return;
    for (const auto& conv : pass.paletteConvs) palette_conv(conv);
    if (pass.discardable) return;
    if (pass.msaaSamples > 1) log_once("a multisampled pass (drawn single-sampled)");
    const auto& scene = pass.colorAttachments[SceneColorAttachmentIndex];
    const uint32_t tw = scene.size.width, th = scene.size.height;
    DkImageView colors[MaxColorAttachments];
    const DkImageView* colorPtrs[MaxColorAttachments];
    uint32_t count = 0;
    for (uint32_t i = 0; i < pass.colorAttachmentCount && i < MaxColorAttachments; i++) {
        View* v = find_view(pass.colorAttachments[i].view.Get());
        if (!v || !v->tex->valid) {
            log_once("a pass whose colour target has no deko3d image (skipped)");
            return;
        }
        colors[i] = target_view(v);
        colorPtrs[i] = &colors[i];
        count++;
    }
    DkImageView depth;
    const DkImageView* depthPtr = nullptr;
    g_passDepth = nullptr;
    g_passColor = nullptr;
    if (count > 0) {
        if (View* cv = find_view(pass.colorAttachments[0].view.Get())) g_passColor = cv->tex;
    }
    if (pass.depthStencilView && (pass.hasDepth || pass.hasStencil)) {
        View* v = find_view(pass.depthStencilView.Get());
        if (v && v->tex->valid) {
            depth = target_view(v);
            depthPtr = &depth;
            g_passDepth = v->tex;
        }
    }
    dkCmdBufBindRenderTargets(R.cmd, colorPtrs, count, depthPtr);
    // load ops (encoding.cpp render()): clears over the whole target
    const DkScissor full = {0, 0, tw, th};
    dkCmdBufSetScissors(R.cmd, 0, &full, 1);
    g_cache.scValid = false;
    for (uint32_t i = 0; i < count; i++) {
        const auto& a = pass.colorAttachments[i];
        const auto load = a.loadOp != wgpu::LoadOp::Undefined ? a.loadOp : (a.clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load);
        if (load == wgpu::LoadOp::Clear) {
            dkCmdBufClearColorFloat(R.cmd, i, DkColorMask_RGBA, a.clearValue.x(), a.clearValue.y(), a.clearValue.z(),
                                    a.clearValue.w());
        }
    }
    if (depthPtr) {
        const auto depthLoad = pass.hasDepth ? (pass.depthLoadOp != wgpu::LoadOp::Undefined
                                                    ? pass.depthLoadOp
                                                    : (pass.clearDepth ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load))
                                             : wgpu::LoadOp::Undefined;
        const bool stencilClear = pass.hasStencil && pass.stencilLoadOp == wgpu::LoadOp::Clear;
        if (depthLoad == wgpu::LoadOp::Clear || stencilClear) {
            dkCmdBufClearDepthStencil(R.cmd, depthLoad == wgpu::LoadOp::Clear, pass.clearDepthValue,
                                      stencilClear ? 0xFF : 0, uint8_t(pass.stencilClearValue));
        }
    }
    g_frame.workSinceTransfer = true;
    for (auto& c : pass.commands) {
        switch (c.type) {
        case CommandType::SetViewport: apply_viewport(c.data.setViewport); break;
        case CommandType::SetScissor: apply_scissor(c.data.setScissor, tw, th); break;
        case CommandType::Draw:
            if (c.data.draw.encoder == nullptr) break;
            switch (draw_kind(c.data.draw.encoder)) {
            case DrawKind::Gx: draw_gx(c.data.draw); break;
            case DrawKind::Clear: draw_clear(c.data.draw, tw, th); break;
            case DrawKind::Other:
                log_once("a draw command of another kind");
                g_stats.skippedOther++;
                break;
            }
            break;
        case CommandType::CustomDraw:
            log_once("a runtime-registered draw type");
            g_stats.skippedOther++;
            break;
        case CommandType::DebugMarker: break;
        }
    }
    // later passes and copies read what this one wrote
    barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    gpu_mark(GpuWork::Efb);
    g_stats.passes++;
    if (pass.resolveTarget) efb_copy(pass);
    if (pass.snapshotColorDst) {
        copy_image(find_texture(pass.copySourceTexture.Get()), 0, 0, 0, find_texture(pass.snapshotColorDst.Get()), 0, 0,
                   0, tw, th);
    }
    if (pass.snapshotDepthDst) depth_snapshot(pass.copySourceDepthView, pass.msaaSamples, pass.snapshotDepthDst);
    if (pass.snapshotNormalDst) {
        copy_image(find_texture(pass.copySourceNormalTexture.Get()), 0, 0, 0, find_texture(pass.snapshotNormalDst.Get()),
                   0, 0, 0, tw, th);
    }
}

void texture_copy(const TextureCopy& c) {
    copy_image(find_texture(c.src.texture.Get()), c.src.mipLevel, c.src.origin.x, c.src.origin.y,
               find_texture(c.dst.texture.Get()), c.dst.mipLevel, c.dst.origin.x, c.dst.origin.y, c.size.width,
               c.size.height);
}

}  // namespace

void invalidate_state() { g_cache = Cache{}; }

void gpu_mark(GpuWork work) {
    if (!g_gpuTimers || !R.frameOpen) return;
    const uint64_t frame = R.frame + 1;
    Marks& m = g_marks[frame % swdk::kFrames];
    if (m.count >= kMaxMarks) {
        g_stats.gpuDropped++;
        return;
    }
    const swdk::QueryRegion q = swdk::query_region(frame);
    dkCmdBufReportCounter(R.cmd, DkCounter_Timestamp, q.gpu + uint64_t(m.count) * 16);
    m.work[m.count++] = work;
}

EncodeStats& encode_stats() { return g_stats; }

bool fixed_draw(const char* module, const char* vsEntry, const char* fsEntry, const DkImageView* target, uint32_t w,
                uint32_t h, bool clear, const FixedBind* binds, uint32_t bindCount) {
    const FixedShader sh = fixed_shader(module, vsEntry, fsEntry);
    if (!sh) {
        char what[160];
        snprintf(what, sizeof what, "fixed shader %s (%s, %s) missing from the DKSH cache", module, vsEntry, fsEntry);
        log_once(what);
        return false;
    }
    if (target) {
        const DkImageView* t[] = {target};
        dkCmdBufBindRenderTargets(R.cmd, t, 1, nullptr);
        set_swizzle(swdk::C.flipY);
        swdk::set_view(0, 0, w, h);
        if (clear) dkCmdBufClearColorFloat(R.cmd, 0, DkColorMask_RGBA, 0.f, 0.f, 0.f, 0.f);
    }
    const DkShader* shaders[] = {&sh.vs->shader, &sh.fs->shader};
    dkCmdBufBindShaders(R.cmd, DkStageFlag_GraphicsMask, shaders, 2);
    swdk::bind_2d_state(false);
    for (const swdk::NamedShader* stage : {sh.vs, sh.fs}) {
        const DkStage dkStage = stage == sh.vs ? DkStage_Vertex : DkStage_Fragment;
        for (const swdk::SlotBinding& b : stage->bindings) {
            for (uint32_t i = 0; i < bindCount; i++) {
                if (binds[i].kind != b.kind || binds[i].binding != b.binding) continue;
                if (b.kind == swdk::SlotBinding::Kind::Uniform) {
                    dkCmdBufBindUniformBuffer(R.cmd, dkStage, b.slot, binds[i].addr, binds[i].size);
                } else if (b.kind == swdk::SlotBinding::Kind::Sampler) {
                    dkCmdBufBindTexture(R.cmd, dkStage, b.slot, binds[i].handle);
                }
            }
        }
    }
    dkCmdBufBindVtxAttribState(R.cmd, nullptr, 0);
    dkCmdBufBindVtxBufferState(R.cmd, nullptr, 0);
    dkCmdBufDraw(R.cmd, DkPrimitive_Triangles, 3, 1, 0, 0);
    if (target) barrier(DkBarrier_Fragments, DkInvalidateFlags_Image);
    invalidate_state();
    g_frame.workSinceTransfer = true;
    return true;
}

StagingMemory staging_create(size_t slot, uint64_t size) {
    char what[64];
    snprintf(what, sizeof what, "Aurora staging slot %zu (vertices, uniforms, indices, storage, uploads)", slot);
    g_staging[slot].block = swdk::block_create(uint32_t(size), what);
    return {g_staging[slot].block.cpu, g_staging[slot].block.gpu};
}

bool staging_ready(size_t slot) {
    const uint64_t frame = g_staging[slot].frame.load(std::memory_order_acquire);
    return frame == 0 || swdk::frames_completed() >= frame;
}

void frame_presented(size_t stagingSlot) {
    g_staging[stagingSlot].frame.store(R.frame, std::memory_order_release);
    g_frame.open = false;
}

void frame_begin(FramePacket& frame) {
    static const bool once = [] {
        g_submitDraws = uint32_t(std::max(0L, swdk::env_long("COS_DK_SUBMIT_DRAWS", 256)));
        g_gpuTimers = swdk::env_flag("COS_DK_GPU_TIMERS", true);
        dklog("encoder: Aurora's frames recorded into deko3d; submits every %u draws (COS_DK_SUBMIT_DRAWS), a "
              "barrier at every pass end, GPU timestamps %s (COS_DK_GPU_TIMERS)",
              g_submitDraws, g_gpuTimers ? "on" : "off");
        return true;
    }();
    (void)once;
    swdk::frame_open();
    objects_frame_start();
    const uint64_t f = R.frame + 1;
    if (f > swdk::kFrames) read_marks(f - swdk::kFrames);  // that frame's fence was waited for in frame_open
    g_marks[f % swdk::kFrames].count = 0;
    g_marks[f % swdk::kFrames].cpuNs = swdk::now_ns();
    g_frame = FrameState{};
    g_frame.open = true;
    // the previous frame's draws may still sample what this frame's first upload overwrites: the copy
    // engine waits for them (the queue orders submissions, not engines)
    g_frame.workSinceTransfer = true;
    g_frame.staging = g_staging[frame.stagingBuffer].block.gpu;
    const swdk::StreamAlloc imm = swdk::stream_alloc(kImmediatesSize, DK_UNIFORM_BUF_ALIGNMENT);
    g_frame.immediates = imm.gpu;
    if (imm) memset(imm.cpu, 0, kImmediatesSize);
    invalidate_state();
    gpu_mark(GpuWork::FrameStart);
}

void encode_op(FramePacket& frame, const FrameOp& op) {
    if (!g_frame.open || !R.frameOpen) frame_begin(frame);
    const uint64_t t0 = swdk::now_ns();
    do_uploads(op);
    switch (op.type) {
    case FrameOpType::RenderPass:
        if (op.renderPass) encode_pass(*op.renderPass);
        break;
    case FrameOpType::TextureCopy:
        if (op.textureCopy) texture_copy(*op.textureCopy);
        break;
    case FrameOpType::EncoderTask:
        if (op.encoderTask) log_once("an encoder task (push_encoder_task)");
        break;
    }
    g_stats.encodeNs += swdk::now_ns() - t0;
}

OverflowUpload overflow_upload(const uint8_t* data, uint32_t bytesPerRow, uint32_t rowsPerImage) {
    const uint32_t rowBytes = (bytesPerRow + 255u) & ~255u;
    swdk::Block b = swdk::block_create(rowBytes * rowsPerImage, "an oversized texture upload");
    for (uint32_t r = 0; r < rowsPerImage; r++) memcpy(b.cpu + size_t(r) * rowBytes, data + size_t(r) * bytesPerRow, bytesPerRow);
    return {b.block, b.gpu};
}

}  // namespace aurora::gfx::dk
