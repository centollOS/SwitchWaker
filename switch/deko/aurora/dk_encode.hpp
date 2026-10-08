#pragma once
// What the encoder (dk_encode.cpp) shares with the present and the readback (dk_present.cpp): the
// fixed passes (a full-target draw with one of the cache's fixed shaders), the state cache's reset,
// the GPU timestamps and the counters of the perf lines.
#include <deko3d.h>

#include <cstdint>

#include "dk_objects.hpp"
#include "dk_pipeline.hpp"

namespace aurora::gfx::dk {

// a binding of a fixed shader, matched with its named record's slots by kind and WGSL binding
struct FixedBind {
    swdk::SlotBinding::Kind kind = swdk::SlotBinding::Kind::Sampler;
    uint32_t binding = 0;
    DkResHandle handle = 0;  // Sampler
    DkGpuAddr addr = 0;      // Uniform
    uint32_t size = 0;
};

// One fullscreen-triangle draw of a fixed shader into a colour target (w x h, cleared to 0 first when
// clear), blending off, depth off; a barrier after it. Null target: into what is bound (the present).
// False (logged once per module) when the shaders are missing.
bool fixed_draw(const char* module, const char* vsEntry, const char* fsEntry, const DkImageView* target, uint32_t w,
                uint32_t h, bool clear, const FixedBind* binds, uint32_t bindCount);

// the encoder's view of the command buffer state is lost (another pass bound its own state)
void invalidate_state();

// GPU time per frame, split by the kind of work that ends at each timestamp (perf-switch gpu line)
enum class GpuWork : uint8_t { FrameStart, Efb, Conversion, Copy, Present, Imgui, Other, Count };
void gpu_mark(GpuWork work);

// counters for cos_switch_stats (aurora_switch_dk_gfx_stats): running totals
struct EncodeStats {
    uint64_t passes = 0, draws = 0, clears = 0, skippedPipeline = 0, skippedShader = 0, skippedOther = 0;
    uint64_t pipelineBinds = 0, shaderBinds = 0, textureBinds = 0, uniformBinds = 0;
    uint64_t uploads = 0, uploadBytes = 0, copies = 0, conversions = 0, barriers = 0, submits = 0;
    uint64_t encodeNs = 0;
    uint64_t gpuFrames = 0, gpuNs[size_t(GpuWork::Count)] = {}, gpuTotalNs = 0, gpuDropped = 0;
};
EncodeStats& encode_stats();

}  // namespace aurora::gfx::dk
