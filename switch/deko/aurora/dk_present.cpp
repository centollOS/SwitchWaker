// The present and the readback of the deko3d NRO's Aurora side (dk_gfx.hpp; docs/DEKO3D_MIGRATION_PLAN.md
// phase 3).
//
// Present (switch/deko frame_present's picture, Aurora's aurora.cpp end_frame for WebGPU): the EFB
// (webgpu::present_source) is resampled to the size it is shown at, area or bilinear as the options
// menu says (webgpu::get_resampler; the present_resample fixed shader), then drawn into the swapchain
// image in the viewport webgpu::calculate_present_viewport fits it to (the aspect fit of patch 0006),
// with the xfb_copy shader, or with the dk_fxaa shader (FXAA, one pass instead of the copy) when the options
// menu's Suavizado de bordes is on (aurora_switch_dk_set_fxaa); ImGui comes after it.
//
// FSR 1 (options menu's Reescalado, aurora_switch_dk_set_rescale): when the EFB is smaller than the shown
// size, stage 0 draws EASU (dk_fsr_easu) in place of present_resample, and the final draw is RCAS
// (dk_fsr_rcas, the sharpening) in place of the copy. With FXAA on the final draw is FXAA alone: FXAA runs
// on the EASU picture and RCAS is skipped (sharpening what FXAA just softened would undo it, and a pass of
// its own would cost a full-screen draw more). An EFB of the shown size skips EASU (RCAS or FXAA read the EFB
// itself); an EFB larger than the shown size is not FSR's case (it is a downscale): the plain resample.
//
// Readback (read_texture): a copy-engine copy into CPU-visible memory on a command buffer of its own,
// after what the frame has recorded so far, and a wait for its fence (~1 ms for the picto box's
// shutter area; COS_SHOT's frames).
#include "dk_encode.hpp"
#include "dk_gfx.hpp"

#include "dk_aurora.h"

#include "gfx/frame.hpp"
#include "webgpu/gpu.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace aurora::gfx::dk {
using swdk::dklog;
using swdk::R;

namespace {

// the resampled picture: an image of our own, at the size the picture is shown
struct Resampled {
    bool valid = false;
    uint32_t width = 0, height = 0;
    DkImage image{};
    swdk::ImageAlloc mem;
    uint32_t slot = 0;
} g_resampled;

struct ResampleUniforms {  // present_resample.wgsl's Uniforms (std140)
    uint32_t samplerMode;
    float frameWidth;
    float frameHeight;
    uint32_t pad;
};

struct EasuUniforms {  // dk_fsr_easu.wgsl's Uniforms (std140)
    float inWidth, inHeight;
    float invInWidth, invInHeight;
};

struct RcasUniforms {  // dk_fsr_rcas.wgsl's Uniforms (std140)
    float invWidth, invHeight;
    float sharpness, pad;  // sharpness: 2^-stops
};

// RCAS's strength in stops (0 sharpest; AMD's default is 0.2)
constexpr float kRcasStops = 0.25f;

// what the final draw reads: the resampled picture, or the EFB itself when FSR skipped EASU
struct PictureIn {
    bool valid = false;
    bool rcas = false;  // FSR 1 is on for this frame
    uint32_t width = 0, height = 0;
    uint32_t slot = 0;
} g_in;

struct FxaaUniforms {  // dk_fxaa.wgsl's Uniforms (std140)
    float invWidth, invHeight;
    float minThreshold, relThreshold;  // the contrast an edge needs
    float subpix, pad;                 // the sub-pixel blend's strength
};

// the options menu's strengths (COS_FXAA 1 Suave, 2 Normal, 3 Fuerte): less blur and fewer edges touched,
// or the reverse
struct FxaaPreset {
    float minThreshold, relThreshold, subpix;
};
constexpr FxaaPreset kFxaaPresets[] = {{0.125f, 0.25f, 0.25f}, {0.0625f, 0.166f, 0.5f}, {0.0312f, 0.125f, 0.75f}};

// FXAA in place of the plain copy (COS_FXAA); written by the game thread, read by the render worker
std::atomic<int> g_fxaa{0};  // 0 off, else the preset + 1
// the options menu's Reescalado (COS_RESCALE): 0 bilinear, 1 área, 2 FSR 1
std::atomic<int> g_rescale{0};

Viewport g_viewport{};

uint32_t extent(float v) { return std::max(1u, static_cast<uint32_t>(std::lround(std::max(v, 1.f)))); }

bool ensure_resampled(uint32_t w, uint32_t h) {
    if (g_resampled.valid && g_resampled.width == w && g_resampled.height == h) return true;
    if (g_resampled.valid) swdk::image_free_later(g_resampled.mem);
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, R.device);
    m.format = DkImageFormat_RGBA8_Unorm;
    m.flags = DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine;
    m.dimensions[0] = w;
    m.dimensions[1] = h;
    swdk::image_tile_size_fix(m, h);
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &m);
    g_resampled.mem = swdk::image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
    dkImageInitialize(&g_resampled.image, &layout, g_resampled.mem.block, g_resampled.mem.offset);
    if (g_resampled.slot == 0) g_resampled.slot = swdk::image_slot_alloc();
    if (g_resampled.slot == 0) return false;
    swdk::write_image_descriptor(g_resampled.slot, g_resampled.image);
    g_resampled.valid = true;
    g_resampled.width = w;
    g_resampled.height = h;
    dklog("present: the picture resampled to %ux%u", w, h);
    return true;
}

// a descriptor of its own for the EFB, written again when its image changes (a resize)
uint32_t source_slot(Tex* src) {
    static uint32_t srcSlot = 0;
    static DkGpuAddr srcAddr = 0;
    const DkGpuAddr addr = dkImageGetGpuAddr(&src->image);
    if (srcAddr != addr || srcSlot == 0) {
        if (srcSlot == 0) srcSlot = swdk::image_slot_alloc();
        if (srcSlot == 0) return 0;
        DkImageView srcView;
        dkImageViewDefaults(&srcView, &src->image);
        swdk::write_image_view_descriptor(srcSlot, srcView);
        srcAddr = addr;
    }
    return srcSlot;
}

void picture(void*, int stage) {
    const auto& source = webgpu::present_source();
    Tex* src = find_texture(source.texture.Get());
    if (stage == 0) {
        g_in.valid = false;
        g_viewport = webgpu::calculate_present_viewport(R.width, R.height, source.size.width, source.size.height);
        if (!src || !src->valid || g_viewport.width <= 0.f || g_viewport.height <= 0.f) return;
        const uint32_t w = extent(g_viewport.width), h = extent(g_viewport.height);
        const uint32_t srcSlot = source_slot(src);
        if (srcSlot == 0) return;
        const bool fsr = g_rescale.load(std::memory_order_relaxed) == 2 && source.size.width <= w &&
                         source.size.height <= h;
        if (fsr && source.size.width == w && source.size.height == h) {
            g_in = {true, true, w, h, srcSlot};  // nothing to upscale: RCAS (or FXAA) reads the EFB
            return;
        }
        if (!ensure_resampled(w, h)) return;
        const swdk::StreamAlloc u = swdk::stream_alloc(256, DK_UNIFORM_BUF_ALIGNMENT);
        if (!u) return;
        const DkResHandle h0 = dkMakeTextureHandle(srcSlot, swdk::kSamplerLinearClamp);
        DkImageView target;
        dkImageViewDefaults(&target, &g_resampled.image);
        const FixedBind binds[] = {
            {swdk::SlotBinding::Kind::Uniform, 0, 0, u.gpu, 256},
            {swdk::SlotBinding::Kind::Sampler, 2, h0},
        };
        bool drawn;
        if (fsr) {
            const EasuUniforms uniforms = {float(source.size.width), float(source.size.height),
                                           1.f / float(source.size.width), 1.f / float(source.size.height)};
            memcpy(u.cpu, &uniforms, sizeof uniforms);
            drawn = fixed_draw("dk_fsr_easu", "vs_main", "fs_main", &target, w, h, true, binds, 2);
        } else {
            const bool area = g_rescale.load(std::memory_order_relaxed) == 1 || webgpu::get_resampler() == SAMPLER_AREA;
            const ResampleUniforms uniforms = {area ? 1u : 0u, float(w), float(h), 0};
            memcpy(u.cpu, &uniforms, sizeof uniforms);
            // present_resample: uniforms 0, the texture with its sampler and again for textureLoad
            drawn = fixed_draw("present_resample", "vs_main", "fs_main", &target, w, h, true, binds, 2);
        }
        (void)drawn;
        g_in = {true, fsr, w, h, g_resampled.slot};
        return;
    }
    if (stage == 1) {
        if (!g_in.valid || g_viewport.width <= 0.f) return;
        // set_present_viewport (aurora.cpp): the viewport, and the scissor around it
        const DkViewport vp = {g_viewport.left, g_viewport.top, g_viewport.width, g_viewport.height, 0.f, 1.f};
        dkCmdBufSetViewports(R.cmd, 0, &vp, 1);
        const auto clampc = [](float v, uint32_t max) {
            return uint32_t(std::clamp(v, 0.f, float(max)));
        };
        const uint32_t x0 = clampc(std::floor(g_viewport.left), R.width), y0 = clampc(std::floor(g_viewport.top), R.height);
        const uint32_t x1 = clampc(std::ceil(g_viewport.left + g_viewport.width), R.width);
        const uint32_t y1 = clampc(std::ceil(g_viewport.top + g_viewport.height), R.height);
        const DkScissor sc = {x0, y0, x1 - x0, y1 - y0};
        dkCmdBufSetScissors(R.cmd, 0, &sc, 1);
        const DkViewportSwizzle sw = {DkSwizzle_PositiveX, swdk::C.flipPresent ? DkSwizzle_NegativeY : DkSwizzle_PositiveY,
                                      DkSwizzle_PositiveZ, DkSwizzle_PositiveW};
        dkCmdBufSetViewportSwizzles(R.cmd, 0, &sw, 1);
        const FixedBind binds[] = {
            {swdk::SlotBinding::Kind::Sampler, 1, dkMakeTextureHandle(g_in.slot, swdk::kSamplerLinearClamp)},
        };
        const int fxaa = g_fxaa.load(std::memory_order_relaxed);
        if (fxaa > 0) {
            const swdk::StreamAlloc u = swdk::stream_alloc(256, DK_UNIFORM_BUF_ALIGNMENT);
            if (u) {
                const FxaaPreset& pr = kFxaaPresets[std::min(fxaa, 3) - 1];
                const FxaaUniforms uniforms = {1.f / float(g_in.width), 1.f / float(g_in.height),
                                               pr.minThreshold, pr.relThreshold, pr.subpix, 0.f};
                memcpy(u.cpu, &uniforms, sizeof uniforms);
                // dk_fxaa: uniforms 0, the texture with its sampler (the layout of present_resample)
                const FixedBind fxaaBinds[] = {
                    {swdk::SlotBinding::Kind::Uniform, 0, 0, u.gpu, 256},
                    {swdk::SlotBinding::Kind::Sampler, 2, dkMakeTextureHandle(g_in.slot, swdk::kSamplerLinearClamp)},
                };
                if (fixed_draw("dk_fxaa", "vs_main", "fs_main", nullptr, 0, 0, false, fxaaBinds, 2)) {
                    gpu_mark(GpuWork::Present);
                    return;
                }
            }
        }
        if (g_in.rcas && fxaa <= 0) {
            const swdk::StreamAlloc u = swdk::stream_alloc(256, DK_UNIFORM_BUF_ALIGNMENT);
            if (u) {
                const RcasUniforms uniforms = {1.f / float(g_in.width), 1.f / float(g_in.height),
                                               std::exp2(-kRcasStops), 0.f};
                memcpy(u.cpu, &uniforms, sizeof uniforms);
                // dk_fsr_rcas: uniforms 0, the texture with its sampler (the layout of present_resample)
                const FixedBind rcasBinds[] = {
                    {swdk::SlotBinding::Kind::Uniform, 0, 0, u.gpu, 256},
                    {swdk::SlotBinding::Kind::Sampler, 2, dkMakeTextureHandle(g_in.slot, swdk::kSamplerLinearClamp)},
                };
                if (fixed_draw("dk_fsr_rcas", "vs_main", "fs_main", nullptr, 0, 0, false, rcasBinds, 2)) {
                    gpu_mark(GpuWork::Present);
                    return;
                }
            }
        }
        fixed_draw("xfb_copy", "vs_main", "fs_opaque", nullptr, 0, 0, false, binds, 1);
        gpu_mark(GpuWork::Present);
        return;
    }
    gpu_mark(GpuWork::Imgui);  // stage 2: after ImGui
}

// ---- readback
DkCmdBuf g_rbCmd = nullptr;
DkMemBlock g_rbCmdMem = nullptr;
constexpr uint32_t kRbCmdSize = 64u << 10;

}  // namespace

void present_init() {
    g_fxaa.store(int(std::clamp(swdk::env_long("COS_FXAA", 0), 0L, 3L)), std::memory_order_relaxed);
    g_rescale.store(int(std::clamp(swdk::env_long("COS_RESCALE", 0), 0L, 2L)), std::memory_order_relaxed);
    swdk::set_picture(picture, nullptr);
}

bool read_texture(WGPUTexture texture, std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height,
                  wgpu::TextureFormat& format, uint32_t mip) {
    Tex* t = find_texture(texture);
    if (!t || !t->valid || t->compressed || t->blockBytes != 4) {
        dklog("readback: texture %p %s", static_cast<void*>(texture),
              !t ? "has no deko3d image" : "is not 4 bytes per texel");
        return false;
    }
    if (mip >= t->mips) return false;
    width = std::max(t->width >> mip, 1u);
    height = std::max(t->height >> mip, 1u);
    format = static_cast<wgpu::TextureFormat>(t->wformat);
    const uint32_t rowBytes = width * 4;
    swdk::Block dst = swdk::block_create(rowBytes * height, "a readback");
    if (!g_rbCmd) {
        DkCmdBufMaker cm;
        dkCmdBufMakerDefaults(&cm, R.device);
        g_rbCmd = dkCmdBufCreate(&cm);
        DkMemBlockMaker mm;
        dkMemBlockMakerDefaults(&mm, R.device, kRbCmdSize);
        g_rbCmdMem = dkMemBlockCreate(&mm);
    }
    // what the frame recorded so far goes first (the copy reads its results)
    if (R.frameOpen) swdk::submit("readback");
    dkCmdBufClear(g_rbCmd);
    dkCmdBufAddMemory(g_rbCmd, g_rbCmdMem, 0, kRbCmdSize);
    dkCmdBufBarrier(g_rbCmd, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_L2Cache);
    const DkImageView v = level_view(t, mip);
    const DkImageRect rect = {0, 0, 0, width, height, 1};
    const DkCopyBuf out = {dst.gpu, rowBytes, rowBytes * height};
    dkCmdBufCopyImageToBuffer(g_rbCmd, &v, &rect, &out, 0);
    dkCmdBufBarrier(g_rbCmd, DkBarrier_Full, DkInvalidateFlags_L2Cache);
    DkFence fence;
    dkCmdBufSignalFence(g_rbCmd, &fence, true);
    const DkCmdList list = dkCmdBufFinishList(g_rbCmd);
    swdk::check_queue("a readback");
    {
        std::lock_guard<std::mutex> lock(swdk::fence_mutex());
        dkQueueSubmitCommands(R.queue, list);
        dkQueueFlush(R.queue);
    }
    const DkResult r = dkFenceWait(&fence, 5'000'000'000ll);
    if (r != DkResult_Success) {
        dklog("readback: the copy of %ux%u did not complete (result %d)", width, height, int(r));
        swdk::block_free_deferred(dst.block);
        return false;
    }
    pixels.assign(dst.cpu, dst.cpu + size_t(rowBytes) * height);
    dkMemBlockDestroy(dst.block);
    return true;
}

}  // namespace aurora::gfx::dk

using namespace aurora::gfx::dk;

extern "C" void aurora_switch_dk_set_fxaa(int level) { g_fxaa.store(std::clamp(level, 0, 3), std::memory_order_relaxed); }

extern "C" void aurora_switch_dk_set_rescale(int mode) { g_rescale.store(std::clamp(mode, 0, 2), std::memory_order_relaxed); }

// The perf lines' figures (cos_switch_stats.cpp, COS_SWITCH_DEKO3D): running totals in the order of
// dk_aurora.h's AuroraSwitchDkStat
extern "C" void aurora_switch_dk_gfx_stats(uint64_t* out, size_t count) {
    const EncodeStats& s = encode_stats();
    const swdk::ShaderTotals sh = swdk::shader_totals();
    const swdk::SubmitStats sub = swdk::submit_stats();
    const ObjectStats o = object_stats();
    const uint64_t v[AURORA_SWITCH_DK_STAT_COUNT] = {
        s.passes,
        s.draws,
        s.clears,
        s.skippedPipeline,
        s.skippedShader + s.skippedOther,
        s.pipelineBinds,
        s.shaderBinds,
        s.textureBinds,
        s.uniformBinds,
        s.uploads,
        s.uploadBytes,
        s.copies,
        s.conversions,
        s.barriers,
        sub.submits,
        sub.frames,
        s.encodeNs,
        s.gpuFrames,
        s.gpuTotalNs,
        s.gpuNs[size_t(GpuWork::Efb)],
        s.gpuNs[size_t(GpuWork::Conversion)],
        s.gpuNs[size_t(GpuWork::Copy)],
        s.gpuNs[size_t(GpuWork::Present)],
        s.gpuNs[size_t(GpuWork::Imgui)],
        s.gpuNs[size_t(GpuWork::Other)] + s.gpuNs[size_t(GpuWork::FrameStart)],
        s.gpuDropped,
        sh.loaded,
        sh.pending,
        sh.failed,
        sh.compiled,
        sh.skippedDraws,
        o.textures,
        o.imageBytes,
    };
    for (size_t i = 0; i < count && i < AURORA_SWITCH_DK_STAT_COUNT; i++) out[i] = v[i];
}

// pc_capture.cpp and pc_shot.cpp (COS_SWITCH_DEKO3D): Aurora's texture level 0 as 4-byte texels
extern "C" bool aurora_switch_dk_read_texture(void* texture, uint8_t** pixels, uint32_t* width, uint32_t* height,
                                              uint32_t* wgpuFormat) {
    swdk::host_alloc_thread();
    static std::vector<uint8_t> buffer;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    if (!read_texture(static_cast<WGPUTexture>(texture), buffer, *width, *height, format)) return false;
    *pixels = buffer.data();
    *wgpuFormat = static_cast<uint32_t>(format);
    return true;
}
