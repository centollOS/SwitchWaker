// The shadows of Aurora's WebGPU textures, views, samplers and bind groups (dk_objects.hpp;
// docs/DEKO3D_MIGRATION_PLAN.md phase 3). The __wrap_ functions below take the place of Dawn's for
// every caller in the NRO (switch/native/CMakeLists.txt: -Wl,--wrap=wgpuDeviceCreateTexture ...):
// each calls Dawn's (__real_) and keeps the shadow in step.
#include "dk_objects.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

extern "C" {
WGPUTexture __real_wgpuDeviceCreateTexture(WGPUDevice device, WGPUTextureDescriptor const* descriptor);
void __real_wgpuTextureAddRef(WGPUTexture texture);
void __real_wgpuTextureRelease(WGPUTexture texture);
WGPUTextureView __real_wgpuTextureCreateView(WGPUTexture texture, WGPUTextureViewDescriptor const* descriptor);
void __real_wgpuTextureViewAddRef(WGPUTextureView view);
void __real_wgpuTextureViewRelease(WGPUTextureView view);
WGPUSampler __real_wgpuDeviceCreateSampler(WGPUDevice device, WGPUSamplerDescriptor const* descriptor);
void __real_wgpuSamplerAddRef(WGPUSampler sampler);
void __real_wgpuSamplerRelease(WGPUSampler sampler);
WGPUBindGroup __real_wgpuDeviceCreateBindGroup(WGPUDevice device, WGPUBindGroupDescriptor const* descriptor);
void __real_wgpuBindGroupAddRef(WGPUBindGroup group);
void __real_wgpuBindGroupRelease(WGPUBindGroup group);
}

namespace aurora::gfx::dk {
using swdk::R;
using swdk::dklog;

namespace {

std::shared_mutex g_mutex;
std::unordered_map<const void*, Tex*> g_textures;
std::unordered_map<const void*, View*> g_views;
std::unordered_map<const void*, Samp*> g_samplers;
std::unordered_map<const void*, BindGroup*> g_bindGroups;
ObjectStats g_stats;  // under g_mutex

// descriptor slots of released views, retired by the render worker
std::mutex g_slotMutex;
std::vector<uint32_t> g_freedSlots;

bool g_compression = false;  // COS_DK_COMPRESSION=1: hardware compression on colour render targets

template <class T> T* find(std::unordered_map<const void*, T*>& map, const void* h) {
    if (!h) return nullptr;
    std::shared_lock<std::shared_mutex> lock(g_mutex);
    auto it = map.find(h);
    return it == map.end() ? nullptr : it->second;
}

const char* label_of(WGPUStringView s) {
    return s.data ? s.data : "";
}

DkImageSwizzle swizzle_of(WGPUComponentSwizzle s, DkImageSwizzle identity) {
    switch (s) {
    case WGPUComponentSwizzle_Zero: return DkImageSwizzle_Zero;
    case WGPUComponentSwizzle_One: return DkImageSwizzle_One;
    case WGPUComponentSwizzle_R: return DkImageSwizzle_Red;
    case WGPUComponentSwizzle_G: return DkImageSwizzle_Green;
    case WGPUComponentSwizzle_B: return DkImageSwizzle_Blue;
    case WGPUComponentSwizzle_A: return DkImageSwizzle_Alpha;
    default: return identity;
    }
}

DkWrapMode wrap_of(WGPUAddressMode m) {
    switch (m) {
    case WGPUAddressMode_Repeat: return DkWrapMode_Repeat;
    case WGPUAddressMode_MirrorRepeat: return DkWrapMode_MirroredRepeat;
    default: return DkWrapMode_ClampToEdge;  // ClampToEdge, Undefined (WebGPU's default)
    }
}

uint64_t fnv_bytes(const void* p, size_t n, uint64_t h = 0xCBF29CE484222325ull) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}

// ---- destruction (the last reference): under no lock; the maps were updated by the caller
void destroy(Tex* t) {
    if (t->valid) swdk::image_free_deferred(t->mem);
    delete t;
}
void release_tex(Tex* t);
void destroy(View* v) {
    if (v->slot) {
        std::lock_guard<std::mutex> lock(g_slotMutex);
        g_freedSlots.push_back(v->slot);
    }
    Tex* t = v->tex;
    delete v;
    if (t) release_tex(t);
}
void release_view(View* v);
void release_sampler(Samp* s);
void destroy(Samp* s) { delete s; }
void destroy(BindGroup* g) {
    for (auto& e : g->entries) {
        if (e.view) release_view(e.view);
        if (e.sampler) release_sampler(e.sampler);
    }
    delete g;
}

template <class T> bool drop(std::unordered_map<const void*, T*>& map, T* obj, uint64_t ObjectStats::*alive) {
    if (obj->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return false;
    {
        std::unique_lock<std::shared_mutex> lock(g_mutex);
        auto it = map.find(obj->handle);
        if (it != map.end() && it->second == obj) map.erase(it);
        g_stats.*alive -= 1;
    }
    return true;
}

void release_tex(Tex* t) {
    if (drop(g_textures, t, &ObjectStats::textures)) {
        {
            std::unique_lock<std::shared_mutex> lock(g_mutex);
            if (t->valid) g_stats.imageBytes -= t->mem.size;
        }
        destroy(t);
    }
}
void release_view(View* v) {
    if (drop(g_views, v, &ObjectStats::views)) destroy(v);
}
void release_sampler(Samp* s) {
    if (drop(g_samplers, s, &ObjectStats::samplers)) destroy(s);
}
void release_bind_group(BindGroup* g) {
    if (drop(g_bindGroups, g, &ObjectStats::bindGroups)) destroy(g);
}

// ---- creation
Tex* make_texture(WGPUTexture handle, const WGPUTextureDescriptor& d) {
    static bool once = [] {
        g_compression = swdk::env_flag("COS_DK_COMPRESSION", false);
        dklog("textures: images in the deko3d image heap; colour render targets %s (COS_DK_COMPRESSION=1)",
              g_compression ? "with hardware compression" : "without hardware compression");
        return true;
    }();
    (void)once;
    auto* t = new Tex;
    t->handle = handle;
    t->wformat = d.format;
    t->width = std::max<uint32_t>(d.size.width, 1);
    t->height = std::max<uint32_t>(d.size.height, 1);
    t->layers = std::max<uint32_t>(d.size.depthOrArrayLayers, 1);
    t->mips = std::max<uint32_t>(d.mipLevelCount, 1);
    t->samples = std::max<uint32_t>(d.sampleCount, 1);
    t->usage = d.usage;
    t->label = label_of(d.label);
    const FormatInfo f = format_info(d.format);
    t->format = f.format;
    t->depth = f.depth;
    t->compressed = f.compressed;
    t->blockW = f.blockW;
    t->blockH = f.blockH;
    t->blockBytes = f.blockBytes;
    if (f.format == DkImageFormat_None) {
        std::unique_lock<std::shared_mutex> lock(g_mutex);
        if (g_stats.unknownFormats++ < 16)
            dklog("textures: WebGPU format 0x%X (%s, %ux%u) has no deko3d format: not drawn", unsigned(d.format),
                  t->label.c_str(), t->width, t->height);
        return t;
    }
    if (t->samples > 1) {
        static int logged = 0;
        if (logged++ < 4)
            dklog("textures: %s asks for %u samples; drawn single-sampled (multisampling is not supported here)",
                  t->label.c_str(), t->samples);
    }
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, R.device);
    m.type = d.dimension == WGPUTextureDimension_3D ? DkImageType_3D
             : t->layers > 1                         ? DkImageType_2DArray
                                                     : DkImageType_2D;
    m.format = f.format;
    m.dimensions[0] = t->width;
    m.dimensions[1] = t->height;
    m.dimensions[2] = m.type == DkImageType_2D ? 0 : t->layers;
    m.mipLevels = t->mips;
    const bool render = (d.usage & WGPUTextureUsage_RenderAttachment) != 0;
    if (render && f.canRender) m.flags |= DkImageFlags_UsageRender;
    if (f.can2d) m.flags |= DkImageFlags_Usage2DEngine;
    if (render && g_compression && !f.depth) m.flags |= DkImageFlags_HwCompression;
    swdk::image_tile_size_fix(m, f.compressed ? (t->height + f.blockH - 1) / f.blockH : t->height);
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &m);
    t->mem = swdk::image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
    dkImageInitialize(&t->image, &layout, t->mem.block, t->mem.offset);
    t->valid = true;
    return t;
}

}  // namespace

FormatInfo format_info(WGPUTextureFormat f) {
    FormatInfo i;
    auto plain = [&](DkImageFormat fmt, uint8_t bytes, bool canRender = true, bool can2d = true) {
        i.format = fmt;
        i.blockBytes = bytes;
        i.canRender = canRender;
        i.can2d = can2d;
    };
    auto depth = [&](DkImageFormat fmt, uint8_t bytes) {
        plain(fmt, bytes, true, false);
        i.depth = true;
    };
    auto block = [&](DkImageFormat fmt, uint8_t w, uint8_t h, uint8_t bytes) {
        i.format = fmt;
        i.blockW = w;
        i.blockH = h;
        i.blockBytes = bytes;
        i.compressed = true;
    };
    switch (f) {
    case WGPUTextureFormat_R8Unorm: plain(DkImageFormat_R8_Unorm, 1); break;
    case WGPUTextureFormat_R8Snorm: plain(DkImageFormat_R8_Snorm, 1); break;
    case WGPUTextureFormat_R8Uint: plain(DkImageFormat_R8_Uint, 1, true, false); break;
    case WGPUTextureFormat_R8Sint: plain(DkImageFormat_R8_Sint, 1, true, false); break;
    case WGPUTextureFormat_R16Unorm: plain(DkImageFormat_R16_Unorm, 2); break;
    case WGPUTextureFormat_R16Snorm: plain(DkImageFormat_R16_Snorm, 2); break;
    case WGPUTextureFormat_R16Uint: plain(DkImageFormat_R16_Uint, 2, true, false); break;
    case WGPUTextureFormat_R16Sint: plain(DkImageFormat_R16_Sint, 2, true, false); break;
    case WGPUTextureFormat_R16Float: plain(DkImageFormat_R16_Float, 2); break;
    case WGPUTextureFormat_RG8Unorm: plain(DkImageFormat_RG8_Unorm, 2); break;
    case WGPUTextureFormat_RG8Snorm: plain(DkImageFormat_RG8_Snorm, 2); break;
    case WGPUTextureFormat_RG8Uint: plain(DkImageFormat_RG8_Uint, 2, true, false); break;
    case WGPUTextureFormat_RG8Sint: plain(DkImageFormat_RG8_Sint, 2, true, false); break;
    case WGPUTextureFormat_R32Float: plain(DkImageFormat_R32_Float, 4); break;
    case WGPUTextureFormat_R32Uint: plain(DkImageFormat_R32_Uint, 4, true, false); break;
    case WGPUTextureFormat_R32Sint: plain(DkImageFormat_R32_Sint, 4, true, false); break;
    case WGPUTextureFormat_RG16Unorm: plain(DkImageFormat_RG16_Unorm, 4); break;
    case WGPUTextureFormat_RG16Snorm: plain(DkImageFormat_RG16_Snorm, 4); break;
    case WGPUTextureFormat_RG16Uint: plain(DkImageFormat_RG16_Uint, 4, true, false); break;
    case WGPUTextureFormat_RG16Sint: plain(DkImageFormat_RG16_Sint, 4, true, false); break;
    case WGPUTextureFormat_RG16Float: plain(DkImageFormat_RG16_Float, 4); break;
    case WGPUTextureFormat_RGBA8Unorm: plain(DkImageFormat_RGBA8_Unorm, 4); break;
    case WGPUTextureFormat_RGBA8UnormSrgb: plain(DkImageFormat_RGBA8_Unorm_sRGB, 4); break;
    case WGPUTextureFormat_RGBA8Snorm: plain(DkImageFormat_RGBA8_Snorm, 4); break;
    case WGPUTextureFormat_RGBA8Uint: plain(DkImageFormat_RGBA8_Uint, 4, true, false); break;
    case WGPUTextureFormat_RGBA8Sint: plain(DkImageFormat_RGBA8_Sint, 4, true, false); break;
    case WGPUTextureFormat_BGRA8Unorm: plain(DkImageFormat_BGRA8_Unorm, 4); break;
    case WGPUTextureFormat_BGRA8UnormSrgb: plain(DkImageFormat_BGRA8_Unorm_sRGB, 4); break;
    case WGPUTextureFormat_RGB10A2Uint: plain(DkImageFormat_RGB10A2_Uint, 4, true, false); break;
    case WGPUTextureFormat_RGB10A2Unorm: plain(DkImageFormat_RGB10A2_Unorm, 4); break;
    case WGPUTextureFormat_RG11B10Ufloat: plain(DkImageFormat_RG11B10_Float, 4); break;
    case WGPUTextureFormat_RGB9E5Ufloat: plain(DkImageFormat_E5BGR9_Float, 4, false, true); break;
    case WGPUTextureFormat_RG32Float: plain(DkImageFormat_RG32_Float, 8); break;
    case WGPUTextureFormat_RG32Uint: plain(DkImageFormat_RG32_Uint, 8, true, false); break;
    case WGPUTextureFormat_RG32Sint: plain(DkImageFormat_RG32_Sint, 8, true, false); break;
    case WGPUTextureFormat_RGBA16Unorm: plain(DkImageFormat_RGBA16_Unorm, 8); break;
    case WGPUTextureFormat_RGBA16Snorm: plain(DkImageFormat_RGBA16_Snorm, 8); break;
    case WGPUTextureFormat_RGBA16Uint: plain(DkImageFormat_RGBA16_Uint, 8, true, false); break;
    case WGPUTextureFormat_RGBA16Sint: plain(DkImageFormat_RGBA16_Sint, 8, true, false); break;
    case WGPUTextureFormat_RGBA16Float: plain(DkImageFormat_RGBA16_Float, 8); break;
    case WGPUTextureFormat_RGBA32Float: plain(DkImageFormat_RGBA32_Float, 16); break;
    case WGPUTextureFormat_RGBA32Uint: plain(DkImageFormat_RGBA32_Uint, 16, true, false); break;
    case WGPUTextureFormat_RGBA32Sint: plain(DkImageFormat_RGBA32_Sint, 16, true, false); break;
    case WGPUTextureFormat_Stencil8: depth(DkImageFormat_S8, 1); break;
    case WGPUTextureFormat_Depth16Unorm: depth(DkImageFormat_Z16, 2); break;
    case WGPUTextureFormat_Depth24Plus: depth(DkImageFormat_Z24X8, 4); break;
    case WGPUTextureFormat_Depth24PlusStencil8: depth(DkImageFormat_Z24S8, 4); break;
    case WGPUTextureFormat_Depth32Float: depth(DkImageFormat_ZF32, 4); break;
    case WGPUTextureFormat_Depth32FloatStencil8: depth(DkImageFormat_ZF32_X24S8, 8); break;
    case WGPUTextureFormat_BC1RGBAUnorm: block(DkImageFormat_RGBA_BC1, 4, 4, 8); break;
    case WGPUTextureFormat_BC1RGBAUnormSrgb: block(DkImageFormat_RGBA_BC1_sRGB, 4, 4, 8); break;
    case WGPUTextureFormat_BC2RGBAUnorm: block(DkImageFormat_RGBA_BC2, 4, 4, 16); break;
    case WGPUTextureFormat_BC2RGBAUnormSrgb: block(DkImageFormat_RGBA_BC2_sRGB, 4, 4, 16); break;
    case WGPUTextureFormat_BC3RGBAUnorm: block(DkImageFormat_RGBA_BC3, 4, 4, 16); break;
    case WGPUTextureFormat_BC3RGBAUnormSrgb: block(DkImageFormat_RGBA_BC3_sRGB, 4, 4, 16); break;
    case WGPUTextureFormat_BC4RUnorm: block(DkImageFormat_R_BC4_Unorm, 4, 4, 8); break;
    case WGPUTextureFormat_BC4RSnorm: block(DkImageFormat_R_BC4_Snorm, 4, 4, 8); break;
    case WGPUTextureFormat_BC5RGUnorm: block(DkImageFormat_RG_BC5_Unorm, 4, 4, 16); break;
    case WGPUTextureFormat_BC5RGSnorm: block(DkImageFormat_RG_BC5_Snorm, 4, 4, 16); break;
    case WGPUTextureFormat_BC6HRGBUfloat: block(DkImageFormat_RGBA_BC6H_UF16_Float, 4, 4, 16); break;
    case WGPUTextureFormat_BC6HRGBFloat: block(DkImageFormat_RGBA_BC6H_SF16_Float, 4, 4, 16); break;
    case WGPUTextureFormat_BC7RGBAUnorm: block(DkImageFormat_RGBA_BC7_Unorm, 4, 4, 16); break;
    case WGPUTextureFormat_BC7RGBAUnormSrgb: block(DkImageFormat_RGBA_BC7_Unorm_sRGB, 4, 4, 16); break;
    case WGPUTextureFormat_ASTC4x4Unorm: block(DkImageFormat_RGBA_ASTC_4x4, 4, 4, 16); break;
    case WGPUTextureFormat_ASTC4x4UnormSrgb: block(DkImageFormat_RGBA_ASTC_4x4_sRGB, 4, 4, 16); break;
    case WGPUTextureFormat_ASTC5x4Unorm: block(DkImageFormat_RGBA_ASTC_5x4, 5, 4, 16); break;
    case WGPUTextureFormat_ASTC5x4UnormSrgb: block(DkImageFormat_RGBA_ASTC_5x4_sRGB, 5, 4, 16); break;
    case WGPUTextureFormat_ASTC5x5Unorm: block(DkImageFormat_RGBA_ASTC_5x5, 5, 5, 16); break;
    case WGPUTextureFormat_ASTC5x5UnormSrgb: block(DkImageFormat_RGBA_ASTC_5x5_sRGB, 5, 5, 16); break;
    case WGPUTextureFormat_ASTC6x5Unorm: block(DkImageFormat_RGBA_ASTC_6x5, 6, 5, 16); break;
    case WGPUTextureFormat_ASTC6x5UnormSrgb: block(DkImageFormat_RGBA_ASTC_6x5_sRGB, 6, 5, 16); break;
    case WGPUTextureFormat_ASTC6x6Unorm: block(DkImageFormat_RGBA_ASTC_6x6, 6, 6, 16); break;
    case WGPUTextureFormat_ASTC6x6UnormSrgb: block(DkImageFormat_RGBA_ASTC_6x6_sRGB, 6, 6, 16); break;
    case WGPUTextureFormat_ASTC8x5Unorm: block(DkImageFormat_RGBA_ASTC_8x5, 8, 5, 16); break;
    case WGPUTextureFormat_ASTC8x5UnormSrgb: block(DkImageFormat_RGBA_ASTC_8x5_sRGB, 8, 5, 16); break;
    case WGPUTextureFormat_ASTC8x6Unorm: block(DkImageFormat_RGBA_ASTC_8x6, 8, 6, 16); break;
    case WGPUTextureFormat_ASTC8x6UnormSrgb: block(DkImageFormat_RGBA_ASTC_8x6_sRGB, 8, 6, 16); break;
    case WGPUTextureFormat_ASTC8x8Unorm: block(DkImageFormat_RGBA_ASTC_8x8, 8, 8, 16); break;
    case WGPUTextureFormat_ASTC8x8UnormSrgb: block(DkImageFormat_RGBA_ASTC_8x8_sRGB, 8, 8, 16); break;
    case WGPUTextureFormat_ASTC10x5Unorm: block(DkImageFormat_RGBA_ASTC_10x5, 10, 5, 16); break;
    case WGPUTextureFormat_ASTC10x5UnormSrgb: block(DkImageFormat_RGBA_ASTC_10x5_sRGB, 10, 5, 16); break;
    case WGPUTextureFormat_ASTC10x6Unorm: block(DkImageFormat_RGBA_ASTC_10x6, 10, 6, 16); break;
    case WGPUTextureFormat_ASTC10x6UnormSrgb: block(DkImageFormat_RGBA_ASTC_10x6_sRGB, 10, 6, 16); break;
    case WGPUTextureFormat_ASTC10x8Unorm: block(DkImageFormat_RGBA_ASTC_10x8, 10, 8, 16); break;
    case WGPUTextureFormat_ASTC10x8UnormSrgb: block(DkImageFormat_RGBA_ASTC_10x8_sRGB, 10, 8, 16); break;
    case WGPUTextureFormat_ASTC10x10Unorm: block(DkImageFormat_RGBA_ASTC_10x10, 10, 10, 16); break;
    case WGPUTextureFormat_ASTC10x10UnormSrgb: block(DkImageFormat_RGBA_ASTC_10x10_sRGB, 10, 10, 16); break;
    case WGPUTextureFormat_ASTC12x10Unorm: block(DkImageFormat_RGBA_ASTC_12x10, 12, 10, 16); break;
    case WGPUTextureFormat_ASTC12x10UnormSrgb: block(DkImageFormat_RGBA_ASTC_12x10_sRGB, 12, 10, 16); break;
    case WGPUTextureFormat_ASTC12x12Unorm: block(DkImageFormat_RGBA_ASTC_12x12, 12, 12, 16); break;
    case WGPUTextureFormat_ASTC12x12UnormSrgb: block(DkImageFormat_RGBA_ASTC_12x12_sRGB, 12, 12, 16); break;
    default: break;
    }
    return i;
}

Tex* find_texture(WGPUTexture t) { return find(g_textures, t); }
View* find_view(WGPUTextureView v) { return find(g_views, v); }
Samp* find_sampler(WGPUSampler s) { return find(g_samplers, s); }
BindGroup* find_bind_group(WGPUBindGroup g) { return find(g_bindGroups, g); }

uint32_t view_slot(View* v) {
    if (!v || !v->tex || !v->tex->valid) return swdk::kEmptyImage;
    if (v->slot == 0) {
        const uint32_t slot = swdk::image_slot_alloc();
        if (slot == 0) return swdk::kEmptyImage;
        v->view.pImage = &v->tex->image;
        swdk::write_image_view_descriptor(slot, v->view);
        v->slot = slot;
    }
    return v->slot;
}

DkResHandle texture_handle(View* v, Samp* s) {
    const uint32_t image = view_slot(v);
    const uint32_t sampler = s ? swdk::sampler_slot(s->sampler, s->key) : uint32_t(swdk::kSamplerNearestClamp);
    return dkMakeTextureHandle(image, sampler);
}

const DkResHandle* gx_handles(BindGroup* g) {
    const uint64_t frame = R.frame + 1;
    if (g->handlesFrame == frame) return g->handles;
    View* views[8] = {};
    Samp* samplers[8] = {};
    for (const auto& e : g->entries) {
        const uint32_t unit = e.binding / 2;
        if (unit >= 8) continue;
        if (e.binding & 1) {
            samplers[unit] = e.sampler;
        } else {
            views[unit] = e.view;
        }
    }
    for (uint32_t i = 0; i < 8; i++) g->handles[i] = texture_handle(views[i], samplers[i]);
    g->handlesFrame = frame;
    return g->handles;
}

void objects_frame_start() {
    std::vector<uint32_t> slots;
    {
        std::lock_guard<std::mutex> lock(g_slotMutex);
        slots.swap(g_freedSlots);
    }
    for (uint32_t s : slots) swdk::image_slot_free(s);
}

DkImageView level_view(Tex* t, uint32_t level) {
    DkImageView v;
    dkImageViewDefaults(&v, &t->image);
    v.mipLevelOffset = uint8_t(level);
    return v;
}

ObjectStats object_stats() {
    std::shared_lock<std::shared_mutex> lock(g_mutex);
    return g_stats;
}

}  // namespace aurora::gfx::dk

using namespace aurora::gfx::dk;

extern "C" {

WGPUTexture __wrap_wgpuDeviceCreateTexture(WGPUDevice device, WGPUTextureDescriptor const* descriptor) {
    WGPUTexture t = __real_wgpuDeviceCreateTexture(device, descriptor);
    if (t && descriptor && R.device) {
        Tex* shadow = make_texture(t, *descriptor);
        std::unique_lock<std::shared_mutex> lock(g_mutex);
        auto [it, inserted] = g_textures.emplace(t, shadow);
        if (!inserted) it->second = shadow;  // (a stale entry: Dawn reused the address)
        g_stats.textures++;
        g_stats.texturesCreated++;
        if (shadow->valid) g_stats.imageBytes += shadow->mem.size;
    }
    return t;
}

void __wrap_wgpuTextureAddRef(WGPUTexture texture) {
    if (Tex* t = find(g_textures, texture)) t->refs.fetch_add(1, std::memory_order_relaxed);
    __real_wgpuTextureAddRef(texture);
}

void __wrap_wgpuTextureRelease(WGPUTexture texture) {
    if (Tex* t = find(g_textures, texture)) release_tex(t);
    __real_wgpuTextureRelease(texture);
}

WGPUTextureView __wrap_wgpuTextureCreateView(WGPUTexture texture, WGPUTextureViewDescriptor const* d) {
    WGPUTextureView v = __real_wgpuTextureCreateView(texture, d);
    Tex* t = find(g_textures, texture);
    if (!v || !t) return v;
    auto* view = new View;
    view->handle = v;
    view->tex = t;
    t->refs.fetch_add(1, std::memory_order_relaxed);
    dkImageViewDefaults(&view->view, &t->image);
    const uint32_t base = d ? d->baseMipLevel : 0;
    uint32_t count = d && d->mipLevelCount != WGPU_MIP_LEVEL_COUNT_UNDEFINED ? d->mipLevelCount : t->mips - base;
    count = std::min(count, t->mips > base ? t->mips - base : 1u);
    view->baseMip = base;
    view->mipCount = count;
    view->view.mipLevelOffset = uint8_t(base);
    view->view.mipLevelCount = uint8_t(count);
    if (d && d->format != WGPUTextureFormat_Undefined && d->format != t->wformat) {
        const FormatInfo f = format_info(d->format);
        if (f.format != DkImageFormat_None) view->view.format = f.format;
    }
    if (d && d->arrayLayerCount != WGPU_ARRAY_LAYER_COUNT_UNDEFINED && t->layers > 1) {
        view->view.layerOffset = uint16_t(d->baseArrayLayer);
        view->view.layerCount = uint16_t(d->arrayLayerCount);
        if (d->dimension == WGPUTextureViewDimension_2D) view->view.type = DkImageType_2D;
    }
    if (d && d->aspect == WGPUTextureAspect_StencilOnly) view->view.dsSource = DkDsSource_Stencil;
    for (const WGPUChainedStruct* c = d ? d->nextInChain : nullptr; c; c = c->next) {
        if (c->sType == WGPUSType_TextureComponentSwizzleDescriptor) {
            const auto* s = reinterpret_cast<const WGPUTextureComponentSwizzleDescriptor*>(c);
            view->view.swizzle[0] = swizzle_of(s->swizzle.r, DkImageSwizzle_Red);
            view->view.swizzle[1] = swizzle_of(s->swizzle.g, DkImageSwizzle_Green);
            view->view.swizzle[2] = swizzle_of(s->swizzle.b, DkImageSwizzle_Blue);
            view->view.swizzle[3] = swizzle_of(s->swizzle.a, DkImageSwizzle_Alpha);
        }
    }
    std::unique_lock<std::shared_mutex> lock(g_mutex);
    g_views[v] = view;
    g_stats.views++;
    return v;
}

void __wrap_wgpuTextureViewAddRef(WGPUTextureView view) {
    if (View* v = find(g_views, view)) v->refs.fetch_add(1, std::memory_order_relaxed);
    __real_wgpuTextureViewAddRef(view);
}

void __wrap_wgpuTextureViewRelease(WGPUTextureView view) {
    if (View* v = find(g_views, view)) release_view(v);
    __real_wgpuTextureViewRelease(view);
}

WGPUSampler __wrap_wgpuDeviceCreateSampler(WGPUDevice device, WGPUSamplerDescriptor const* d) {
    WGPUSampler s = __real_wgpuDeviceCreateSampler(device, d);
    if (!s || !R.device) return s;
    auto* samp = new Samp;
    samp->handle = s;
    DkSampler& k = samp->sampler;
    memset(&k, 0, sizeof k);  // (the key hashes its bytes, padding included)
    dkSamplerDefaults(&k);
    WGPUSamplerDescriptor def = WGPU_SAMPLER_DESCRIPTOR_INIT;
    const WGPUSamplerDescriptor& sd = d ? *d : def;
    k.wrapMode[0] = wrap_of(sd.addressModeU);
    k.wrapMode[1] = wrap_of(sd.addressModeV);
    k.wrapMode[2] = wrap_of(sd.addressModeW);
    k.magFilter = sd.magFilter == WGPUFilterMode_Linear ? DkFilter_Linear : DkFilter_Nearest;
    k.minFilter = sd.minFilter == WGPUFilterMode_Linear ? DkFilter_Linear : DkFilter_Nearest;
    k.mipFilter = sd.mipmapFilter == WGPUMipmapFilterMode_Linear ? DkMipFilter_Linear : DkMipFilter_Nearest;
    // deko3d raises the max to the min when min > max where GL's state tracker swaps them (SwitchWakerHD
    // descriptors.cpp): keep them ordered
    k.lodClampMin = std::min(sd.lodMinClamp, sd.lodMaxClamp);
    k.lodClampMax = std::max(sd.lodMinClamp, sd.lodMaxClamp);
    k.maxAnisotropy = float(std::max<uint16_t>(sd.maxAnisotropy, 1));
    if (sd.compare != WGPUCompareFunction_Undefined) {
        k.compareEnable = true;
        k.compareOp = DkCompareOp(DkCompareOp_Never + (int(sd.compare) - int(WGPUCompareFunction_Never)));
    }
    samp->key = fnv_bytes(&k, sizeof k);
    std::unique_lock<std::shared_mutex> lock(g_mutex);
    g_samplers[s] = samp;
    g_stats.samplers++;
    return s;
}

void __wrap_wgpuSamplerAddRef(WGPUSampler sampler) {
    if (Samp* s = find(g_samplers, sampler)) s->refs.fetch_add(1, std::memory_order_relaxed);
    __real_wgpuSamplerAddRef(sampler);
}

void __wrap_wgpuSamplerRelease(WGPUSampler sampler) {
    if (Samp* s = find(g_samplers, sampler)) release_sampler(s);
    __real_wgpuSamplerRelease(sampler);
}

WGPUBindGroup __wrap_wgpuDeviceCreateBindGroup(WGPUDevice device, WGPUBindGroupDescriptor const* d) {
    WGPUBindGroup g = __real_wgpuDeviceCreateBindGroup(device, d);
    if (!g || !d || !R.device) return g;
    auto* group = new BindGroup;
    group->handle = g;
    for (size_t i = 0; i < d->entryCount; i++) {
        const WGPUBindGroupEntry& e = d->entries[i];
        if (!e.textureView && !e.sampler) continue;
        BindGroup::Entry entry;
        entry.binding = e.binding;
        entry.view = find(g_views, e.textureView);
        entry.sampler = find(g_samplers, e.sampler);
        // the bind group holds its views and samplers as Dawn's does
        if (entry.view) entry.view->refs.fetch_add(1, std::memory_order_relaxed);
        if (entry.sampler) entry.sampler->refs.fetch_add(1, std::memory_order_relaxed);
        group->entries.push_back(entry);
    }
    std::unique_lock<std::shared_mutex> lock(g_mutex);
    g_bindGroups[g] = group;
    g_stats.bindGroups++;
    return g;
}

void __wrap_wgpuBindGroupAddRef(WGPUBindGroup group) {
    if (BindGroup* g = find(g_bindGroups, group)) g->refs.fetch_add(1, std::memory_order_relaxed);
    __real_wgpuBindGroupAddRef(group);
}

void __wrap_wgpuBindGroupRelease(WGPUBindGroup group) {
    if (BindGroup* g = find(g_bindGroups, group)) release_bind_group(g);
    __real_wgpuBindGroupRelease(group);
}

}  // extern "C"
