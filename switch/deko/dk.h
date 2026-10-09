// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/dk.h.
#pragma once
// The deko3d renderer of the Switch NRO (docs/DEKO3D_MIGRATION_PLAN.md, phases 2-3): one device, one
// graphics queue, one command buffer that only Aurora's render worker records into, per-frame rings
// with a fence per frame, the image heap, the shader code block and the descriptor sets. Namespace
// swdk, as switch/deko's shader files. switch/deko/aurora (compiled into Aurora) records Aurora's
// frames with it.
//
// Device conventions (plan section 3.1): DkDeviceFlags_OriginUpperLeft | DkDeviceFlags_DepthZeroToOne:
// window and image row 0 at the top, clip-space z from 0 to 1, clip-space y up (deko3d 0.5.0's
// viewport transform has scaleY = -height/2 with this origin, so normalized y = +1 is row 0;
// SwitchWakerHD dk.h:6-10). That is WebGPU's clip space, so Aurora's WGSL keeps its y through Tint
// (dawn-switch-tint-position-y-up.patch); COS_DK_TEST_PATTERN=1 shows all of it in one picture, and
// the Conventions switches below turn each part around without a rebuild if the console says
// otherwise.
#include <deko3d.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "shader_translate.h"  // SlotBinding: what each WGSL binding of a fixed shader became

struct ImDrawData;

namespace swdk {

// ---- memory (memory.cpp) -------------------------------------------------------------------------
// Per-frame resources come in kFrames slots; a slot is reused after its fence (the frame kFrames
// earlier) has signalled. Aurora's own frame data (vertices, uniforms, indices, storage, texture
// uploads: 63 MiB per frame) lives in its staging blocks (staging_block_create), which the FIFO
// thread writes and the GPU reads in place; the stream slice holds the renderer's own per-frame data
// (the immediates block, ImGui's vertices, the present's uniforms).
constexpr uint32_t kFrames = 3;
constexpr uint32_t kStreamSliceSize = 8u << 20;    // per frame: the renderer's own data
constexpr uint32_t kCmdSliceSize = 4u << 20;       // per frame: command memory
constexpr uint32_t kCmdChunk = 64u << 10;          // fed at frame_begin, then as the command buffer asks
constexpr uint32_t kImageChunkSize = 64u << 20;    // image heap chunks
constexpr uint32_t kCodeSize = 32u << 20;          // shader code (DKSH), bump allocated, never freed
constexpr uint32_t kImageDescriptors = 8192, kSamplerDescriptors = 1024;
constexpr uint32_t kQuerySize = 64u << 10;         // GPU timestamps (kFrames regions)
constexpr uint32_t kQueueCommandMemory = 1u << 20; // the queue's own command memory
// Thread priorities: 0x3B at most. SwitchWakerHD's first deko3d NRO died at start on a thread created
// with 0x3C, which the NPDM refuses (SwitchWakerHD shaders_dk.cpp:77-79); the uam worker uses this.
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
// a memory block of its own, CPU-uncached and GPU-cached (Aurora's staging, oversized uploads)
struct Block {
    DkMemBlock block = nullptr;
    uint8_t* cpu = nullptr;
    DkGpuAddr gpu = 0;
    uint32_t size = 0;
    explicit operator bool() const { return block != nullptr; }
};

void memory_init();
void frame_begin(uint64_t frame);   // waits for the slot's fence, resets its stream and command memory
void frame_end();                   // the fence after this frame's commands (recorded into the command buffer)
bool frame_done(uint64_t frame);    // the GPU has finished that frame's commands (no wait; render worker)
// Any thread: the newest frame the GPU has finished (polls the fences; never waits)
uint64_t frames_completed();
// held while the queue writes the fences (every submit) and while they are polled
std::mutex& fence_mutex();
StreamAlloc stream_alloc(uint32_t size, uint32_t alignment);  // this frame's slice; empty when full (logged)
// Thread-safe: the FIFO thread creates Aurora's textures
ImageAlloc image_alloc(uint32_t size, uint32_t alignment);
// Before dkImageLayoutInitialize of a block-linear 2D image whose level 0 is `rows` rows tall
// (compressed: rows of blocks): deko3d 0.5.0 picks a tile its level-0 layout then shrinks, so 6-8-row
// images (BC 24x24..32x32) overlap their next level unless the tile is set explicitly (SwitchWakerHD
// memory.cpp; the HD packs' BC textures need it, every image goes through it).
void image_tile_size_fix(DkImageLayoutMaker& m, uint32_t rows);
void image_free_later(const ImageAlloc& a);  // render worker: freed when the GPU is done with the current frame
// Any thread: freed once the render worker's next frame has started and the GPU has finished it
void image_free_deferred(const ImageAlloc& a);
// Any thread (logged; null when the heap is exhausted)
Block block_create(uint32_t size, const char* what);
void block_free_deferred(DkMemBlock b);  // any thread: destroyed as image_free_deferred frees
// Copies DKSH into the code block and initializes the shader; false (logged) if full or invalid.
// Thread-safe.
bool code_load(DkShader& shader, const void* dksh, uint32_t size, const char* name);
// whether code_load wrote code since the last call (render worker: then the GPU's shader caches are invalidated
// before the next shader bind, so they cannot keep stale lines of the new code's addresses, e.g. prefetched)
bool code_take_written();
// the "[dk] shader scheduling:" line: COS_DK_SHADER_SCHED (diagnostic, memory.cpp) and what it patched
void sched_report();
uint32_t code_used();
DkGpuAddr image_descriptors();
DkGpuAddr sampler_descriptors();
// the current frame slot's part of the query block: kQuerySize / kFrames bytes, 16 per report
struct QueryRegion {
    uint8_t* cpu = nullptr;
    DkGpuAddr gpu = 0;
    uint32_t size = 0;
};
QueryRegion query_region(uint64_t frame);
struct MemoryStats {
    uint64_t cmdBytesMax = 0, cmdBytesSum = 0;  // command memory fed per frame (64 KB steps)
    uint64_t cmdOverflows = 0;                  // frames that needed more than kCmdSliceSize
    uint64_t streamBytesMax = 0, streamFull = 0;
    uint64_t frames = 0;
    uint64_t imageBytes = 0, imageChunks = 0, codeBytes = 0, blockBytes = 0;
};
MemoryStats memory_stats_take();  // per-frame figures since the last call; totals as they are

// ---- descriptors (descriptors.cpp) -----------------------------------------------------------------
// Image slots below kReservedImages and sampler slots below kReservedSamplers belong to the
// renderer's own passes; the rest are handed out (Aurora's texture views and samplers) and come back
// once the GPU has finished the frame that freed them. Writes are commands (dkCmdBufPushData),
// ordered with the draws. Render worker only.
constexpr uint32_t kOverlayFirstImage = 1, kOverlayImages = 63;  // ImGui's textures: slots 1-63
constexpr uint32_t kPatternImage = 64;                           // the test pattern's texture
constexpr uint32_t kEmptyImage = 65;                             // a 1x1 black texture (missing views)
constexpr uint32_t kReservedImages = 128;
enum : uint32_t { kSamplerLinearClamp = 0, kSamplerNearestClamp = 1, kReservedSamplers = 16 };
void descriptors_frame_start();  // binds the sets; writes the reserved samplers once; retires slots
void write_image_descriptor(uint32_t slot, const DkImage& image);
void write_image_view_descriptor(uint32_t slot, const DkImageView& view);
uint32_t image_slot_alloc();      // a free slot at or above kReservedImages; 0 when none (logged)
void image_slot_free(uint32_t slot);
// A sampler slot for this sampler (key: a hash of it), written now if new; slots are kept by key and
// the least recently used one is reused when all are taken (kSamplerNearestClamp if none can be).
uint32_t sampler_slot(const DkSampler& sampler, uint64_t key);
struct DescriptorStats {
    uint64_t imageWrites = 0, samplerWrites = 0, samplerEvictions = 0;
    uint32_t imagesUsed = 0, samplersUsed = 0;
};
DescriptorStats descriptor_stats();

// ---- the device (device.cpp) ---------------------------------------------------------------------
struct Renderer {
    DkDevice device = nullptr;
    DkQueue queue = nullptr;
    DkCmdBuf cmd = nullptr;  // the render worker's command buffer (one list per frame, or more: submit)
    uint64_t frame = 0;      // frames presented
    uint32_t width = 1280, height = 720;  // the swapchain images (the window)
    bool zcull = false;      // COS_DK_ZCULL=1 (queue flag; off by default, plan section 7.5)
    bool frameOpen = false;  // between frame_open and the present
};
extern Renderer R;

// The conventions the phase 2 test pattern checks (plan section 3.1), each switchable from settings.ini [dev] so
// that the first console session can correct them without a rebuild; all logged at start
// ("[dk] conventions:"). The defaults are the plan's: WebGPU's clip space and facing as they are.
struct Conventions {
    bool flipY = false;        // COS_DK_FLIP_Y=1: every game pass drawn with clip-space y negated
                               // (a viewport swizzle; the winding on screen flips with it)
    bool flipFront = false;    // COS_DK_FLIP_FRONT=1: Aurora's front face inverted (CW <-> CCW)
    bool flipTexture = false;  // COS_DK_FLIP_TEXTURE=1: texture uploads written bottom row first
    bool flipPresent = false;  // COS_DK_FLIP_PRESENT=1: the picture shown upside down at the present
};
extern Conventions C;

// The frame's commands: frame_open (render worker, at Aurora's frame start) waits for the slot, binds
// the descriptor sets and uploads ImGui's textures; submit sends what is recorded so far (the frame
// goes on in the same slot); frame_present ends it.
void frame_open();
void submit(const char* why);
// the game's picture (Aurora's present, switch/deko/aurora), or null: called with stage 0 before the
// swapchain image is bound (offscreen passes: the resample), then with stage 1 into the bound image,
// and with stage 2 after ImGui
using PictureFn = void (*)(void* user, int stage);
void frame_present(const ImDrawData* ui, PictureFn picture, void* user);
// the picture aurora_switch_dk_present (Aurora's present) draws under ImGui
void set_picture(PictureFn picture, void* user);
struct SubmitStats {
    uint64_t submits = 0, frames = 0;
};
SubmitStats submit_stats();

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

// ---- shaders (shaders.cpp, shader_cache.cpp) ---------------------------------------------------------
// Every DKSH the renderer can draw with, by (stage, XXH3 of the GLSL uam compiled): the offline
// cache (initial_dksh_cache.bin next to the NRO, built from the pipeline database), this console's
// own (native/user/cache/dksh_local.bin, what the uam worker compiled for the pipelines the offline
// cache lacked) and the misses compiled during the session. Plan section 6.
void shader_cache_init(uint32_t gxConfigVersion);
// A fixed shader of the file's named records (e.g. "dk_test_pattern.vs_main") with its slots; null
// when absent.
struct ShaderEntry;
struct NamedShader {
    DkShader shader;
    std::vector<SlotBinding> bindings;
    ShaderEntry* entry = nullptr;  // its registry entry (Ready)
};
const NamedShader* shader_cache_named(const char* name);
const char* shader_cache_status();  // a short line for the test pattern's legend

// One stage's shader. state: Missing (no GLSL yet), Queued / Compiling (the uam worker has it),
// Compiled (DKSH waiting for the render worker's load budget), Ready (loaded: `shader` is valid),
// Failed (uam refused it; the draws that need it stay skipped).
enum class ShaderState : uint8_t { Missing, Queued, Compiled, Ready, Failed };
struct ShaderEntry {
    ShaderStage stage = ShaderStage::Vertex;
    uint64_t glslHash = 0;
    std::atomic<ShaderState> state{ShaderState::Missing};
    std::atomic<uint32_t> skippedDraws{0};  // draws skipped waiting for it (the worker's queue order)
    DkShader shader{};
    std::vector<uint8_t> dksh;  // Compiled: the uam output until it is loaded
};
// The entry of a stage's GLSL hash, created (Missing) if new. Stable pointers, never freed.
ShaderEntry* shader_entry(ShaderStage stage, uint64_t glslHash);
// Aurora's shader hash of a GX module -> its two stages' GLSL hashes (both 0 when unknown)
struct ModuleHashes {
    uint64_t vertex = 0, fragment = 0;
};
ModuleHashes shader_module(uint64_t moduleHash);
void shader_module_add(uint64_t moduleHash, uint64_t vertexGlslHash, uint64_t fragmentGlslHash);
// Queues a Missing entry's GLSL on the uam worker (any thread; foreground first). True if queued now.
bool shader_compile(ShaderEntry* e, const std::string& glsl, bool foreground);
// Blocks until the entry is Compiled, Ready or Failed (the compile thread's wait for its pipelines,
// which keeps Aurora's warm-up and loading screen measuring real build times). False if Failed.
bool shader_wait(ShaderEntry* e);
// Render worker: the entry is drawable now; a Compiled one is loaded if this frame's budget
// (COS_DK_SHADER_BUDGET, default 64; 0 = no limit) allows. Counts a skipped draw otherwise.
bool shader_usable(ShaderEntry* e);
void shaders_frame_start();  // the budget's frame (render worker)
// Starts the uam worker and reads dksh_local.bin (after shader_cache_init).
void shaders_init(uint32_t gxConfigVersion);
// "[cos] shaders:" (the perf window's line): loaded, pending, failed, compiled this session, skipped
// draws, code memory; returns the bytes written to out
int shaders_report(char* out, size_t size);
struct ShaderTotals {
    uint64_t loaded = 0, pending = 0, failed = 0, compiled = 0, compileNs = 0, compileMaxNs = 0, skippedDraws = 0,
             loadsThisSession = 0;
};
ShaderTotals shader_totals();

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
// the calling thread (render worker, uam worker) allocates host memory, never the game's JKRHeaps
void host_alloc_thread();
[[noreturn]] void fatal(const char* format, ...) __attribute__((format(printf, 1, 2)));
// The queue is in an error state after a GPU fault, and deko3d aborts on any call that touches it:
// checked before each submit, acquire and present.
void check_queue(const char* before);
// a run option switch (settings.ini [dev]): unset or empty -> def, "0" -> false, anything else -> true
bool env_flag(const char* name, bool def);
long env_long(const char* name, long def);

inline uint64_t now_ns() {
    uint64_t ticks;
    asm volatile("mrs %0, cntpct_el0" : "=r"(ticks));
    return ticks * 625 / 12;  // 19.2 MHz
}

}  // namespace swdk
