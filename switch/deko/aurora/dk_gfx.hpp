#pragma once
// The deko3d renderer's side of Aurora's gfx layer (docs/DEKO3D_MIGRATION_PLAN.md phase 3): what
// Aurora's Switch patches 0013-0014 call with AURORA_GFX_DEKO3D (the deko3d NRO). Aurora still
// records each frame as it does for WebGPU (FramePacket: render passes of draw commands, texture
// uploads, copies), with its uniform packing, caches and pipeline cache unchanged, and Dawn's Null
// device keeps its object model (dk_objects.hpp); these functions turn the recorded frame into
// deko3d commands on the render worker instead of Dawn's encoders:
//   frame       Aurora's staging buffers are deko3d memory the FIFO thread writes and the GPU reads in
//               place (no copies); a staging slot is reused once the GPU has finished its frame
//   passes      render targets, load-op clears, viewport, scissor, blend constant, a barrier at
//               every pass end; palette conversions before their pass, EFB copies (copy engine) and
//               conversions (draws with the fixed shaders) after it
//   draws       GX draws (pipeline state cache, shaders, uniform buffer at its dynamic offset, the
//               64-byte immediates in uniform buffer 15, storage buffers 0 and 1 with their regions'
//               sizes, the 8 texture handles of the bind group, u16 indices) and the partial-clear
//               draws
//   pipelines   built on Aurora's compile thread: the module's DKSH from the caches, or WGSL -> Tint ->
//               post-pass -> the uam worker for a miss; the GX fixed state as gx::pipeline_state_desc
//               gives it
//   present     the EFB resampled (area or bilinear) and drawn with the aspect fit, under ImGui
//   readback    read_texture (the picto box, COS_SHOT)
#include <cstddef>
#include <cstdint>
#include <vector>

#include <webgpu/webgpu_cpp.h>

namespace aurora::gfx {
struct RenderTargetLayout;
namespace detail {
struct FramePacket;
struct FrameOp;
}  // namespace detail
namespace clear {
struct PipelineConfig;
}
}  // namespace aurora::gfx
namespace aurora::gx {
struct PipelineConfig;
struct PipelineOptions;
}  // namespace aurora::gx

namespace aurora::gfx::dk {

// ---- frame (frame.cpp hooks)
struct StagingMemory {
    uint8_t* cpu = nullptr;
    uint64_t gpu = 0;
};
// Aurora's staging slot `slot` (game thread, gfx::initialize): its memory, size bytes
StagingMemory staging_create(size_t slot, uint64_t size);
// any thread: the GPU has finished the last frame that read the slot
bool staging_ready(size_t slot);
// render worker: the frame being recorded reads the slot (begin_frame's work item)
void frame_begin(detail::FramePacket& frame);
// render worker: one sealed pass, copy or task with the uploads queued before it (encode_op)
void encode_op(detail::FramePacket& frame, const detail::FrameOp& op);
// render worker, after the present (Aurora's end_frame work item): the staging slot's frame
void frame_presented(size_t stagingSlot);
// registers the picture callback with switch/deko (gfx::initialize)
void present_init();

// ---- pipelines (Aurora's compile thread)
// A deko3d pipeline (an opaque const Pipe*) for one GX pass of a config, or null if its shaders
// cannot be made (logged)
const void* build_gx_pipeline(const gx::PipelineConfig& config, const RenderTargetLayout& layout,
                              const gx::PipelineOptions& options);
const void* build_clear_pipeline(const clear::PipelineConfig& config, const RenderTargetLayout& layout);
// gx/pipeline.cpp's choice for a destination alpha with source-alpha blending: dual-source blending
// (COS_DK_DUAL_SOURCE=1, once the console showed uam's second output reaches the blender) or the
// alpha prepass (the default)
bool dual_source_blending();

// ---- uploads larger than a frame's texture staging (recording.cpp queue_texture_upload_data)
struct OverflowUpload {
    void* block = nullptr;  // a DkMemBlock, freed once the GPU has used it
    uint64_t gpu = 0;
};
OverflowUpload overflow_upload(const uint8_t* data, uint32_t bytesPerRow, uint32_t rowsPerImage);

// ---- readback (render worker): level 0 of a texture, tightly packed rows of 4-byte texels; false
// (logged) if the texture is unknown or not 4 bytes per texel. Waits for the GPU.
bool read_texture(WGPUTexture texture, std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height,
                  wgpu::TextureFormat& format, uint32_t mip = 0);

}  // namespace aurora::gfx::dk
