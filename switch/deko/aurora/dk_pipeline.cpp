// deko3d pipelines for Aurora (dk_pipeline.hpp; docs/DEKO3D_MIGRATION_PLAN.md sections 3.3 and 6), on
// Aurora's compile thread. A GX pass of a config is Aurora's shader module (its hash: xxh3 of the
// destination alpha mode, the normal attachment and the ShaderConfig, as gx/shader.cpp computes it)
// plus build_pipeline's fixed state:
//   - the module's two stages come from the DKSH caches by the module records (initial_dksh_cache.bin,
//     dksh_local.bin); a miss generates the WGSL (gx::build_shader_source), translates each stage with
//     Tint and the post-pass (shader_translate.cpp), and waits for the uam worker, so the compile
//     thread, not the render worker, takes the 70-430 ms, and Aurora's warm-up and loading screen
//     see a real build time (plan section 6, "Boot");
//   - the state is gx::pipeline_state_desc's (the WebGPU values build_pipeline gives Dawn), converted.
// Clear pipelines use the fixed shaders clear_color / clear_depth of the cache's named records.
#include "dk_pipeline.hpp"

#include "dk_gfx.hpp"

#include "dksh_file.h"
#include "gfx/clear.hpp"
#include "gx/gx.hpp"
#include "gx/pipeline.hpp"
#include "webgpu/gpu.hpp"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>
#include <mutex>
#include <string>

namespace aurora::gfx::dk {
using swdk::dklog;
using swdk::ShaderEntry;
using swdk::ShaderStage;
using swdk::ShaderState;

namespace {

std::mutex g_statsMutex;
PipelineStats g_stats;
std::mutex g_translateMutex;  // Tint, called from one thread anyway

DkCompareOp compare_op(wgpu::CompareFunction f) {
    switch (f) {
    case wgpu::CompareFunction::Never: return DkCompareOp_Never;
    case wgpu::CompareFunction::Less: return DkCompareOp_Less;
    case wgpu::CompareFunction::Equal: return DkCompareOp_Equal;
    case wgpu::CompareFunction::LessEqual: return DkCompareOp_Lequal;
    case wgpu::CompareFunction::Greater: return DkCompareOp_Greater;
    case wgpu::CompareFunction::NotEqual: return DkCompareOp_NotEqual;
    case wgpu::CompareFunction::GreaterEqual: return DkCompareOp_Gequal;
    default: return DkCompareOp_Always;
    }
}

DkBlendFactor blend_factor(wgpu::BlendFactor f, bool alpha) {
    switch (f) {
    case wgpu::BlendFactor::Zero: return DkBlendFactor_Zero;
    case wgpu::BlendFactor::One: return DkBlendFactor_One;
    case wgpu::BlendFactor::Src: return DkBlendFactor_SrcColor;
    case wgpu::BlendFactor::OneMinusSrc: return DkBlendFactor_InvSrcColor;
    case wgpu::BlendFactor::SrcAlpha: return DkBlendFactor_SrcAlpha;
    case wgpu::BlendFactor::OneMinusSrcAlpha: return DkBlendFactor_InvSrcAlpha;
    case wgpu::BlendFactor::Dst: return DkBlendFactor_DstColor;
    case wgpu::BlendFactor::OneMinusDst: return DkBlendFactor_InvDstColor;
    case wgpu::BlendFactor::DstAlpha: return DkBlendFactor_DstAlpha;
    case wgpu::BlendFactor::OneMinusDstAlpha: return DkBlendFactor_InvDstAlpha;
    case wgpu::BlendFactor::SrcAlphaSaturated: return DkBlendFactor_SrcAlphaSaturate;
    // WebGPU's constant is the blend constant's rgb for the colour, its a for the alpha
    case wgpu::BlendFactor::Constant: return alpha ? DkBlendFactor_ConstAlpha : DkBlendFactor_ConstColor;
    case wgpu::BlendFactor::OneMinusConstant: return alpha ? DkBlendFactor_InvConstAlpha : DkBlendFactor_InvConstColor;
    case wgpu::BlendFactor::Src1: return DkBlendFactor_Src1Color;
    case wgpu::BlendFactor::OneMinusSrc1: return DkBlendFactor_InvSrc1Color;
    case wgpu::BlendFactor::Src1Alpha: return DkBlendFactor_Src1Alpha;
    case wgpu::BlendFactor::OneMinusSrc1Alpha: return DkBlendFactor_InvSrc1Alpha;
    default: return DkBlendFactor_One;
    }
}

DkBlendOp blend_op(wgpu::BlendOperation o) {
    switch (o) {
    case wgpu::BlendOperation::Subtract: return DkBlendOp_Sub;
    case wgpu::BlendOperation::ReverseSubtract: return DkBlendOp_RevSub;
    case wgpu::BlendOperation::Min: return DkBlendOp_Min;
    case wgpu::BlendOperation::Max: return DkBlendOp_Max;
    default: return DkBlendOp_Add;
    }
}

// zeroed first: the encoder compares states byte for byte
Pipe* new_pipe() {
    auto* p = new Pipe;
    memset(&p->rasterizer, 0, sizeof p->rasterizer);
    memset(&p->color, 0, sizeof p->color);
    memset(&p->colorWrite, 0, sizeof p->colorWrite);
    memset(&p->blend, 0, sizeof p->blend);
    memset(&p->depthStencil, 0, sizeof p->depthStencil);
    dkRasterizerStateDefaults(&p->rasterizer);
    dkColorStateDefaults(&p->color);
    dkColorWriteStateDefaults(&p->colorWrite);
    dkBlendStateDefaults(&p->blend);
    dkDepthStencilStateDefaults(&p->depthStencil);
    return p;
}

void set_blend(Pipe* p, const wgpu::BlendState& b) {
    dkBlendStateSetOps(&p->blend, blend_op(b.color.operation), blend_op(b.alpha.operation));
    dkBlendStateSetFactors(&p->blend, blend_factor(b.color.srcFactor, false), blend_factor(b.color.dstFactor, false),
                           blend_factor(b.alpha.srcFactor, true), blend_factor(b.alpha.dstFactor, true));
}

const char* stage_tag(ShaderStage s) { return s == ShaderStage::Vertex ? "vs" : "fs"; }

// the module's entry for a stage: from the caches, or translated now and queued on the uam worker
struct StageGlsl {
    uint64_t hash = 0;
    std::string glsl;
};

int g_missLogs = 0;

}  // namespace

namespace {
// COS_DK_WGSL_PATCH (diagnostic): "<module hex>:<find>=><replace>[|<find>=><replace>...]" rewrites that module's
// WGSL before Tint and uam compile it here (the caches' entry is skipped), to test a shader variant on the console
struct WgslPatch {
    uint64_t module = 0;
    std::vector<std::pair<std::string, std::string>> edits;
};
const WgslPatch& wgsl_patch() {
    static const WgslPatch p = [] {
        WgslPatch r;
        const char* e = getenv("COS_DK_WGSL_PATCH");
        if (!e || !*e) return r;
        std::string s = e;
        const size_t colon = s.find(':');
        if (colon == std::string::npos) return r;
        r.module = strtoull(s.substr(0, colon).c_str(), nullptr, 16);
        std::string rest = s.substr(colon + 1);
        size_t at = 0;
        while (at <= rest.size()) {
            const size_t bar = rest.find('|', at);
            const std::string item = rest.substr(at, bar == std::string::npos ? std::string::npos : bar - at);
            const size_t arrow = item.find("=>");
            if (arrow != std::string::npos) r.edits.emplace_back(item.substr(0, arrow), item.substr(arrow + 2));
            if (bar == std::string::npos) break;
            at = bar + 1;
        }
        dklog("pipelines: COS_DK_WGSL_PATCH: module %016llx, %zu edits", (unsigned long long)r.module, r.edits.size());
        return r;
    }();
    return p;
}

}  // namespace

bool dual_source_blending() {
    static const bool on = [] {
        const bool env = swdk::env_flag("COS_DK_DUAL_SOURCE", false);
        const bool device = webgpu::g_dualSourceBlendingSupported;
        dklog("pipelines: destination alpha with source-alpha blending by %s (COS_DK_DUAL_SOURCE=%d; the device "
              "feature %s)",
              env && device ? "dual-source blending" : "an alpha prepass (two draws)", int(env),
              device ? "is offered" : "is not offered");
        return env && device;
    }();
    return on;
}

bool pipe_ready(const Pipe* p) {
    if (!p) return false;
    const bool vs = swdk::shader_usable(p->vs);
    const bool fs = swdk::shader_usable(p->fs);
    return vs && fs;
}

FixedShader fixed_shader(const char* module, const char* vsEntry, const char* fsEntry) {
    const std::string m = module;
    return {swdk::shader_cache_named((m + "." + vsEntry).c_str()), swdk::shader_cache_named((m + "." + fsEntry).c_str())};
}

const void* build_gx_pipeline(const gx::PipelineConfig& config, const RenderTargetLayout& layout,
                              const gx::PipelineOptions& options) {
    // the module, as gx::build_shader names it
    uint32_t normalAttachment = UINT32_MAX;
    for (uint32_t i = 0; i < layout.colorAttachmentCount; ++i) {
        if (layout.colorAttachments[i].semantic == ColorAttachmentSemantic::Normal) normalAttachment = i;
    }
    const auto mode = options.dstAlphaMode;
    const uint64_t moduleHash = xxh3_hash(mode, xxh3_hash(normalAttachment, xxh3_hash(config.shaderConfig)));
    swdk::ModuleHashes hashes = swdk::shader_module(moduleHash);
    ShaderEntry* vs = hashes.vertex ? swdk::shader_entry(ShaderStage::Vertex, hashes.vertex) : nullptr;
    ShaderEntry* fs = hashes.fragment ? swdk::shader_entry(ShaderStage::Fragment, hashes.fragment) : nullptr;
    const auto usable = [](ShaderEntry* e) {
        if (!e) return false;
        const ShaderState s = e->state.load(std::memory_order_acquire);
        return s == ShaderState::Ready || s == ShaderState::Compiled || s == ShaderState::Queued;
    };
    bool miss = false;
    const bool patched = wgsl_patch().module == moduleHash && moduleHash != 0;
    if (patched || !usable(vs) || !usable(fs)) {
        // a miss: WGSL -> GLSL here, DKSH from the uam worker
        miss = true;
        const uint64_t t0 = swdk::now_ns();
        std::string wgsl = gx::build_shader_source(config.shaderConfig, mode, normalAttachment);
        if (patched) {
            int applied = 0;
            for (const auto& [from, to] : wgsl_patch().edits) {
                for (size_t at = wgsl.find(from); at != std::string::npos; at = wgsl.find(from, at + to.size())) {
                    wgsl.replace(at, from.size(), to);
                    applied++;
                }
            }
            dklog("pipelines: COS_DK_WGSL_PATCH: module %016llx rewritten (%d replacements)",
                  (unsigned long long)moduleHash, applied);
        }
        swdk::TranslateResult tv, tf;
        {
            std::lock_guard<std::mutex> lock(g_translateMutex);
            tv = swdk::wgsl_to_glsl(wgsl, "vs_main", ShaderStage::Vertex, swdk::BindingScheme::GX);
            tf = swdk::wgsl_to_glsl(wgsl, "fs_main", ShaderStage::Fragment, swdk::BindingScheme::GX);
        }
        const uint64_t t1 = swdk::now_ns();
        if (!tv.ok || !tf.ok) {
            std::lock_guard<std::mutex> lock(g_statsMutex);
            g_stats.translateFailures++;
            if (g_stats.translateFailures <= 20)
                dklog("pipelines: module %016llx does not translate: %s", (unsigned long long)moduleHash,
                      (!tv.ok ? tv.error : tf.error).substr(0, 300).c_str());
            return nullptr;
        }
        const uint64_t vh = swdk::glsl_hash(tv.glsl), fh = swdk::glsl_hash(tf.glsl);
        swdk::shader_module_add(moduleHash, vh, fh);
        vs = swdk::shader_entry(ShaderStage::Vertex, vh);
        fs = swdk::shader_entry(ShaderStage::Fragment, fh);
        const bool qv = swdk::shader_compile(vs, tv.glsl, true);
        const bool qf = swdk::shader_compile(fs, tf.glsl, true);
        const bool okV = swdk::shader_wait(vs), okF = swdk::shader_wait(fs);
        const uint64_t t2 = swdk::now_ns();
        {
            std::lock_guard<std::mutex> lock(g_statsMutex);
            g_stats.misses++;
            g_stats.translateNs += t1 - t0;
            g_stats.waitNs += t2 - t1;
            if (!okV || !okF) g_stats.compileFailures++;
        }
        if (g_missLogs++ < 64) {
            dklog("pipelines: module %016llx (dst alpha %d) not in the caches: Tint %.1f ms, uam %.0f ms (%s%s%s)%s",
                  (unsigned long long)moduleHash, int(mode), double(t1 - t0) / 1e6, double(t2 - t1) / 1e6,
                  qv ? "vs" : "", qv && qf ? " + " : "", qf ? "fs" : qv ? "" : "both stages known",
                  okV && okF ? "" : ": uam FAILED, its draws stay skipped");
        }
        if (!okV || !okF) return nullptr;
    }
    // the fixed state: build_pipeline's
    const gx::PipelineStateDesc d = gx::pipeline_state_desc(config, layout, options);
    Pipe* p = new_pipe();
    p->vs = vs;
    p->fs = fs;
    p->moduleHash = moduleHash;
    DkRasterizerState& rs = p->rasterizer;
    rs.cullMode = d.primitive.cullMode == wgpu::CullMode::Front  ? DkFace_Front
                  : d.primitive.cullMode == wgpu::CullMode::Back ? DkFace_Back
                                                                 : DkFace_None;
    const bool ccw = d.primitive.frontFace == wgpu::FrontFace::CCW;
    rs.frontFace = ccw != swdk::C.flipFront ? DkFrontFace_CCW : DkFrontFace_CW;
    const float units = float(d.depthStencil.depthBias), slope = d.depthStencil.depthBiasSlopeScale;
    rs.depthBiasEnableMask = d.hasDepth && (units != 0.0f || slope != 0.0f) ? DkPolygonFlag_All : 0;
    p->depthBias[0] = units;
    p->depthBias[1] = d.depthStencil.depthBiasClamp;
    p->depthBias[2] = slope;
    DkDepthStencilState& ds = p->depthStencil;
    ds.depthTestEnable = d.hasDepth;
    ds.depthWriteEnable = d.hasDepth && d.depthStencil.depthWriteEnabled == wgpu::OptionalBool::True;
    ds.depthCompareOp = compare_op(d.depthStencil.depthCompare);
    // the scene colour blends (WebGPU always gets the GX blend state there), other targets do not
    dkColorStateSetBlendEnable(&p->color, SceneColorAttachmentIndex, true);
    for (uint32_t i = 0; i < d.colorCount && i < 8; i++)
        dkColorWriteStateSetMask(&p->colorWrite, i, uint32_t(d.writeMasks[i]) & 0xF);
    set_blend(p, d.blend);
    {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        g_stats.gx++;
        if (!miss) g_stats.cacheHits++;
    }
    return p;
}

const void* build_clear_pipeline(const clear::PipelineConfig& config, const RenderTargetLayout& layout) {
    const bool color = config.clearColor || config.clearAlpha;
    const FixedShader sh = fixed_shader(color ? "clear_color" : "clear_depth");
    if (!sh) {
        static int logged = 0;
        if (logged++ < 4)
            dklog("pipelines: no %s shaders in the DKSH cache: partial clears are not drawn",
                  color ? "clear_color" : "clear_depth");
        return nullptr;
    }
    Pipe* p = new_pipe();
    p->vs = sh.vs->entry;
    p->fs = sh.fs->entry;
    p->rasterizer.cullMode = DkFace_None;
    const bool hasDepth = layout.depthStencilFormat != wgpu::TextureFormat::Undefined;
    p->depthStencil.depthTestEnable = hasDepth;
    p->depthStencil.depthWriteEnable = hasDepth && config.clearDepth;
    p->depthStencil.depthCompareOp = DkCompareOp_Always;
    // clear.cpp: the blend constant is the colour (Constant, Zero); the write mask picks the channels
    dkColorStateSetBlendEnable(&p->color, SceneColorAttachmentIndex, true);
    dkBlendStateSetOps(&p->blend, DkBlendOp_Add, DkBlendOp_Add);
    dkBlendStateSetFactors(&p->blend, DkBlendFactor_ConstColor, DkBlendFactor_Zero, DkBlendFactor_ConstAlpha,
                           DkBlendFactor_Zero);
    for (uint32_t i = 0; i < layout.colorAttachmentCount && i < 8; i++) dkColorWriteStateSetMask(&p->colorWrite, i, 0);
    uint32_t mask = (config.clearColor ? DkColorMask_RGB : 0u) | (config.clearAlpha ? DkColorMask_A : 0u);
    dkColorWriteStateSetMask(&p->colorWrite, SceneColorAttachmentIndex, mask);
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_stats.clear++;
    return p;
}

PipelineStats pipeline_stats() {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    return g_stats;
}

}  // namespace aurora::gfx::dk
