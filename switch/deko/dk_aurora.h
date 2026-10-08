// The deko3d renderer's C entry points (docs/DEKO3D_MIGRATION_PLAN.md, phases 2-3). The deko3d NRO
// (COS_SWITCH_RENDERER=deko3d) builds Aurora with AURORA_GFX_DEKO3D: Aurora records every frame as
// for WebGPU, Dawn's Null device keeps its object model without GPU work, and switch/deko owns the
// GPU: the device, the swapchain on the default NWindow, the recording of Aurora's frames
// (switch/deko/aurora), the present pass (the game's picture or the test pattern, ImGui: the options
// menu, the loading screen, the FPS panel) and the DKSH caches. Aurora's Switch patches 0013-0014
// call these; the harness's platform layer (switch/native/source) reads the statistics.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ImDrawData;

// Device, queue, memory, swapchain, the renderer's own shaders, the DKSH caches and the uam worker.
// From webgpu::initialize on the game thread, before the render worker presents. gxConfigVersion is
// Aurora's gx::GXPipelineConfigVersion (part of the DKSH caches' compiler id).
void aurora_switch_dk_init(uint32_t gxConfigVersion);

// The frame's end into the next swapchain image: clear, the game's picture (or the test pattern with
// COS_DK_TEST_PATTERN=1), ImGui's draw data (may be null), submit and present. Render worker only.
void aurora_switch_dk_present(const struct ImDrawData* ui);

// An RGBA8 texture for ImGui (the font atlas, aurora_imgui_add_texture): the ImTextureID to draw it
// with. Any thread; the pixels are copied now and reach the GPU at the next frame.
uint64_t aurora_switch_dk_imgui_texture(uint32_t width, uint32_t height, const void* rgba8);

// Waits for the GPU and releases the window (Aurora's shutdown).
void aurora_switch_dk_shutdown(void);

// The perf lines' figures (cos_switch_stats.cpp): running totals since the start, in this order.
enum AuroraSwitchDkStat {
    AURORA_SWITCH_DK_PASSES,
    AURORA_SWITCH_DK_DRAWS,
    AURORA_SWITCH_DK_CLEAR_DRAWS,
    AURORA_SWITCH_DK_SKIPPED_NO_PIPELINE,  // the pipeline is not built yet
    AURORA_SWITCH_DK_SKIPPED_SHADER,       // a stage is still compiling or over the load budget
    AURORA_SWITCH_DK_PIPELINE_BINDS,
    AURORA_SWITCH_DK_SHADER_BINDS,
    AURORA_SWITCH_DK_TEXTURE_BINDS,
    AURORA_SWITCH_DK_UNIFORM_BINDS,
    AURORA_SWITCH_DK_UPLOADS,
    AURORA_SWITCH_DK_UPLOAD_BYTES,
    AURORA_SWITCH_DK_COPIES,
    AURORA_SWITCH_DK_CONVERSIONS,
    AURORA_SWITCH_DK_BARRIERS,
    AURORA_SWITCH_DK_SUBMITS,
    AURORA_SWITCH_DK_FRAMES,
    AURORA_SWITCH_DK_ENCODE_NS,
    AURORA_SWITCH_DK_GPU_FRAMES,  // frames with GPU timestamps read back
    AURORA_SWITCH_DK_GPU_TOTAL_NS,
    AURORA_SWITCH_DK_GPU_EFB_NS,
    AURORA_SWITCH_DK_GPU_CONVERSION_NS,
    AURORA_SWITCH_DK_GPU_COPY_NS,
    AURORA_SWITCH_DK_GPU_PRESENT_NS,
    AURORA_SWITCH_DK_GPU_IMGUI_NS,
    AURORA_SWITCH_DK_GPU_OTHER_NS,
    AURORA_SWITCH_DK_GPU_DROPPED,
    AURORA_SWITCH_DK_SHADERS_LOADED,
    AURORA_SWITCH_DK_SHADERS_PENDING,
    AURORA_SWITCH_DK_SHADERS_FAILED,
    AURORA_SWITCH_DK_SHADERS_COMPILED,
    AURORA_SWITCH_DK_SHADER_SKIPPED_DRAWS,
    AURORA_SWITCH_DK_TEXTURES,
    AURORA_SWITCH_DK_TEXTURE_BYTES,
    AURORA_SWITCH_DK_STAT_COUNT
};
void aurora_switch_dk_gfx_stats(uint64_t* out, size_t count);

// "[cos] shaders:" for the perf window: the bytes written to out
int aurora_switch_dk_shaders_report(char* out, size_t size);

// Level 0 of an Aurora texture (a WGPUTexture) as rows of 4-byte texels, read back by the GPU's copy
// engine; render worker only (pc_capture.cpp, pc_shot.cpp). *pixels stays valid until the next call.
// wgpuFormat: the texture's WGPUTextureFormat. False (logged) when it cannot be read.
bool aurora_switch_dk_read_texture(void* texture, uint8_t** pixels, uint32_t* width, uint32_t* height,
                                   uint32_t* wgpuFormat);

#ifdef __cplusplus
}
#endif
