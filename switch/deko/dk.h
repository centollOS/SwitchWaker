// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/dk.h.
#pragma once
// The deko3d renderer of the Switch NRO (docs/DEKO3D_MIGRATION_PLAN.md, phase 2): one device, one
// graphics queue, one command buffer that only Aurora's render worker records into, per-frame rings
// with a fence per frame, the image heap, the shader code block and the descriptor sets. Namespace
// swdk, as switch/deko's shader files.
//
// Device conventions (plan section 3.1): DkDeviceFlags_OriginUpperLeft | DkDeviceFlags_DepthZeroToOne:
// window and image row 0 at the top, clip-space z from 0 to 1, clip-space y up (deko3d 0.5.0's
// viewport transform has scaleY = -height/2 with this origin, so normalized y = +1 is row 0;
// SwitchWakerHD dk.h:6-10). That is WebGPU's clip space, so Aurora's WGSL keeps its y through Tint
// (dawn-switch-tint-position-y-up.patch); COS_DK_TEST_PATTERN=1 shows all of it in one picture.
#include <deko3d.h>

#include <cstdarg>
#include <cstdint>
#include <vector>

#include "shader_translate.h"  // SlotBinding: what each WGSL binding of a fixed shader became

struct ImDrawData;

namespace swdk {

// ---- memory (memory.cpp) -------------------------------------------------------------------------
// Per-frame resources come in kFrames slots; a slot is reused after its fence (the frame kFrames
// earlier) has signalled. Sized for Aurora's frame data (plan section 3.1: vertices 5, uniforms 24,
// indices 2, storage 8 MiB per frame) so the set-up cost logged in phase 2 is the one phase 3 pays.
constexpr uint32_t kFrames = 3;
constexpr uint32_t kStreamSliceSize = 40u << 20;   // per frame: vertices, indices, uniforms, small uploads
constexpr uint32_t kStagingSize = 32u << 20;       // texture uploads (phase 3; allocated now for the cost)
constexpr uint32_t kCmdSliceSize = 4u << 20;       // per frame: command memory
constexpr uint32_t kCmdChunk = 64u << 10;          // fed at frame_begin, then as the command buffer asks
constexpr uint32_t kImageChunkSize = 64u << 20;    // image heap chunks
constexpr uint32_t kCodeSize = 32u << 20;          // shader code (DKSH), bump allocated, never freed
constexpr uint32_t kImageDescriptors = 8192, kSamplerDescriptors = 1024;
constexpr uint32_t kQuerySize = 64u << 10;         // counters and timestamps (phase 4)
constexpr uint32_t kQueueCommandMemory = 1u << 20; // the queue's own command memory
// Thread priorities: 0x3B at most. SwitchWakerHD's first deko3d NRO died at start on a thread created
// with 0x3C, which the NPDM refuses (SwitchWakerHD shaders_dk.cpp:77-79); phase 3's uam worker uses this.
constexpr int kMaxThreadPriority = 0x3B;

struct StreamAlloc {
    void* cpu = nullptr;
    DkGpuAddr gpu = 0;
    explicit operator bool() const { return cpu != nullptr; }
};
// a suballocation of the image heap
struct ImageAlloc {
    DkMemBlock block = nullptr;
    uint32_t offset = 0, size = 0;
    int chunk = -1;
};

void memory_init();
void frame_begin(uint64_t frame);   // waits for the slot's fence, resets its stream and command memory
void frame_end();                   // the fence after this frame's commands (recorded into the command buffer)
bool frame_done(uint64_t frame);    // the GPU has finished that frame's commands (no wait)
StreamAlloc stream_alloc(uint32_t size, uint32_t alignment);  // this frame's slice; empty when full (logged)
ImageAlloc image_alloc(uint32_t size, uint32_t alignment);
// Before dkImageLayoutInitialize of a block-linear 2D image whose level 0 is `rows` rows tall
// (compressed: rows of blocks): deko3d 0.5.0 picks a tile its level-0 layout then shrinks, so 6-8-row
// images (BC 24x24..32x32) overlap their next level unless the tile is set explicitly (SwitchWakerHD
// memory.cpp; phase 4's HD packs need it, every image goes through it from the start).
void image_tile_size_fix(DkImageLayoutMaker& m, uint32_t rows);
void image_free_later(const ImageAlloc& a);  // freed when the GPU is done with the current frame
// Copies DKSH into the code block and initializes the shader; false (logged) if full or invalid.
bool code_load(DkShader& shader, const void* dksh, uint32_t size, const char* name);
uint32_t code_used();
DkGpuAddr image_descriptors();
DkGpuAddr sampler_descriptors();
struct MemoryStats {
    uint64_t cmdBytesMax = 0, cmdBytesSum = 0;  // command memory fed per frame (64 KB steps)
    uint64_t cmdOverflows = 0;                  // frames that needed more than kCmdSliceSize
    uint64_t streamBytesMax = 0, streamFull = 0;
    uint64_t frames = 0;
    uint64_t imageBytes = 0, imageChunks = 0, codeBytes = 0;
};
MemoryStats memory_stats_take();  // per-frame figures since the last call; totals as they are

// ---- descriptors (descriptors.cpp) -----------------------------------------------------------------
// Image slots below kReservedImages and sampler slots below kReservedSamplers belong to the renderer's
// own passes; the rest are handed out (phase 3: Aurora's textures) and come back once the GPU has
// finished the frame that freed them. Writes are commands (dkCmdBufPushData), ordered with the draws.
constexpr uint32_t kOverlayFirstImage = 1, kOverlayImages = 63;  // ImGui's textures: slots 1-63
constexpr uint32_t kPatternImage = 64;                           // the test pattern's texture
constexpr uint32_t kReservedImages = 128;
enum : uint32_t { kSamplerLinearClamp = 0, kSamplerNearestClamp = 1, kReservedSamplers = 16 };
void descriptors_frame_start();  // binds the sets; writes the reserved samplers once; retires slots
void write_image_descriptor(uint32_t slot, const DkImage& image);
uint32_t image_slot_alloc();      // a free slot at or above kReservedImages; 0 when none (logged)
void image_slot_free(uint32_t slot);
struct DescriptorStats {
    uint64_t imageWrites = 0, samplerWrites = 0;
    uint32_t imagesUsed = 0;
};
DescriptorStats descriptor_stats();

// ---- the device (device.cpp) ---------------------------------------------------------------------
struct Renderer {
    DkDevice device = nullptr;
    DkQueue queue = nullptr;
    DkCmdBuf cmd = nullptr;  // the render worker's command buffer (one list per frame)
    uint64_t frame = 0;      // frames presented
    uint32_t width = 1280, height = 720;  // the swapchain images (the window)
    bool zcull = false;      // COS_DK_ZCULL=1 (queue flag; off by default, plan section 7.5)
};
extern Renderer R;

// ---- the renderer's own shaders, embedded DKSH compiled by uam at build time (shaders/*.glsl)
enum ShaderId { kImguiVs, kImguiFs, kTextVs, kTextFs, kShaderCount };
const DkShader* builtin_shader(ShaderId id);  // null if it did not load (logged)

// Viewport and scissor over a window rectangle (pixels from the top left).
void set_view(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
// Rasterizer (no culling), blend (straight alpha) and depth state for the renderer's 2D passes.
void bind_2d_state(bool blend);

// ---- ImGui (overlay.cpp): the options menu, the loading screen and the FPS panel -------------------
void overlay_upload_pending();  // textures aurora_switch_dk_imgui_texture queued, before any draw
void overlay_draw(const ImDrawData* d);
uint32_t overlay_textures();

// ---- the DKSH cache (shader_cache.cpp) -------------------------------------------------------------
// initial_dksh_cache.bin next to the NRO (COS_SWITCH_NRO_DIR), read whole and loaded into the code
// block at start; one "[dk] shader cache:" line. Phase 3 draws Aurora's pipelines from it.
void shader_cache_init(uint32_t gxConfigVersion);
// A fixed shader of the file's named records (e.g. "dk_test_pattern.vs_main") with its slots; null
// when absent.
struct NamedShader {
    DkShader shader;
    std::vector<SlotBinding> bindings;
};
const NamedShader* shader_cache_named(const char* name);
const char* shader_cache_status();  // a short line for the test pattern's legend

// ---- the test pattern (pattern.cpp), COS_DK_TEST_PATTERN=1 ------------------------------------------
bool pattern_enabled();
void pattern_init();  // its texture (render worker, inside a frame)
void pattern_draw();  // into the bound swapchain image and depth buffer
// 3x5 pixel text at (left, top) window pixels, `scale` pixels per font pixel (the pattern's legend)
void draw_text(int left, int top, int scale, const char* const* lines, int count, const float fg[4],
               const float bg[4]);

// ---- logging -------------------------------------------------------------------------------------
void dklog(const char* format, ...) __attribute__((format(printf, 1, 2)));
// The log written out to the SD card now: deko3d aborts without a word when a creation fails
// (release library: 2359-xxxx; SwitchWakerHD backend.cpp:141-150), so creations log what they are
// about to do and flush first.
void log_flush();
[[noreturn]] void fatal(const char* format, ...) __attribute__((format(printf, 1, 2)));
// The queue is in an error state after a GPU fault, and deko3d aborts on any call that touches it:
// checked before each submit, acquire and present.
void check_queue(const char* before);

inline uint64_t now_ns() {
    uint64_t ticks;
    asm volatile("mrs %0, cntpct_el0" : "=r"(ticks));
    return ticks * 625 / 12;  // 19.2 MHz
}

}  // namespace swdk
