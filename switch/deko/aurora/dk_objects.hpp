#pragma once
// The deko3d side of Aurora's WebGPU objects (docs/DEKO3D_MIGRATION_PLAN.md phase 3). The deko3d NRO
// keeps Dawn's Null device as Aurora's object model (textures, views, samplers, bind groups are
// still created, validated and reference-counted by Dawn, with no GPU work), and every texture, view,
// sampler and bind group Aurora creates gets a shadow here, through the linker's --wrap of the
// WebGPU C functions that create them and count their references (switch/native/CMakeLists.txt):
//   texture      -> a DkImage in the image heap (format, size, levels, usage from the descriptor)
//   texture view -> a DkImageView of it (levels, format, swizzle) and, once sampled, an image
//                   descriptor slot
//   sampler      -> a DkSampler (its descriptor slot comes from descriptors.cpp's cache by key)
//   bind group   -> its entries' shadows, for the 8 texture handles of a GX draw
// The shadows mirror Dawn's lifetimes: one count per WebGPU reference, plus the references Dawn's
// objects hold on each other (a view on its texture, a bind group on its views and samplers), so a
// shadow lives exactly as long as the object Aurora can still reach. The last release frees the
// image memory and the descriptor slot once the GPU is past the frames that used them.
//
// Thread-safety: creation and release from any thread (Aurora creates textures on its FIFO thread);
// lookups from the render worker. Descriptor slots are the render worker's.
#include <deko3d.h>
#include <webgpu/webgpu.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "dk.h"

namespace aurora::gfx::dk {

struct Tex {
    std::atomic<int32_t> refs{1};
    WGPUTexture handle = nullptr;
    WGPUTextureFormat wformat = WGPUTextureFormat_Undefined;
    DkImageFormat format = DkImageFormat_None;
    uint32_t width = 0, height = 0, layers = 1, mips = 1, samples = 1;
    uint64_t usage = 0;
    bool depth = false, compressed = false, valid = false;
    uint8_t blockW = 1, blockH = 1, blockBytes = 4;
    DkImage image{};
    swdk::ImageAlloc mem;
    std::string label;
};

struct View {
    std::atomic<int32_t> refs{1};
    WGPUTextureView handle = nullptr;
    Tex* tex = nullptr;
    uint32_t baseMip = 0, mipCount = 1;
    DkImageView view{};  // pImage set to &tex->image
    uint32_t slot = 0;   // the image descriptor (render worker; 0 until first sampled)
};

struct Samp {
    std::atomic<int32_t> refs{1};
    WGPUSampler handle = nullptr;
    DkSampler sampler{};
    uint64_t key = 0;
};

struct BindGroup {
    std::atomic<int32_t> refs{1};
    WGPUBindGroup handle = nullptr;
    struct Entry {
        uint32_t binding = 0;
        View* view = nullptr;
        Samp* sampler = nullptr;
    };
    std::vector<Entry> entries;
    // the GX layout's 8 combined texture handles (binding 2i: view, 2i + 1: sampler), made by the
    // render worker once per frame
    DkResHandle handles[8] = {};
    uint64_t handlesFrame = 0;
};

// Lookups (render worker). Null when the object has no shadow (created before the device existed,
// or not a WebGPU object of this process).
Tex* find_texture(WGPUTexture t);
View* find_view(WGPUTextureView v);
Samp* find_sampler(WGPUSampler s);
BindGroup* find_bind_group(WGPUBindGroup g);

// Render worker: the view's image descriptor slot, written on first use (kEmptyImage if none is
// free or the view is invalid)
uint32_t view_slot(View* v);
// A combined handle for a view and a sampler (null sampler: the nearest-clamp one)
DkResHandle texture_handle(View* v, Samp* s);
// The GX bind group's 8 handles, valid for the frame being recorded
const DkResHandle* gx_handles(BindGroup* g);

// Render worker, at the frame's start: descriptor slots of views released since are retired
void objects_frame_start();

// a DkImageView of one level of a texture (transfers, render targets)
DkImageView level_view(Tex* t, uint32_t level);

struct ObjectStats {
    uint64_t textures = 0, views = 0, samplers = 0, bindGroups = 0;  // alive
    uint64_t texturesCreated = 0, imageBytes = 0, unknownFormats = 0;
};
ObjectStats object_stats();

// The deko3d format of a WebGPU texture format (DkImageFormat_None when unsupported), with its block
// geometry (compressed formats: 4x4 blocks)
struct FormatInfo {
    DkImageFormat format = DkImageFormat_None;
    uint8_t blockW = 1, blockH = 1, blockBytes = 4;
    bool depth = false, compressed = false, canRender = false, can2d = false;
};
FormatInfo format_info(WGPUTextureFormat f);

}  // namespace aurora::gfx::dk
