// The picto box's screen capture (mDoGph_screenCapture, m_Do_graphic.cpp; bug B36). On GameCube
// GXCopyTex writes the photo's part of the EFB into main memory as an I8 (the picto box) or
// RGB565 (the deluxe picto box) texture, and the capture thread (mCaptureProc, encode_s3tc) reads
// it on the CPU to compress it to CMPR, the picture the box shows and saves. Aurora keeps the copy
// on the GPU only (lib/dolphin/gx/GXFrameBuffer.cpp copy_tex: a texture named by the destination
// address), so the buffer stayed as mDoGph_allocFromAny cleared it: encode_s3tc's
// JUT_ASSERT(16 <= i8low && i8high <= 235) panicked on the zeros (an I8 copy is the luma
// 16..235), and the deluxe box would have saved black photos.
//
// pc_gph_capture_ready holds the capture at step 3 for one frame: the frame of the copy asks for
// the readback; pc_capture_frame_end, right after that frame's aurora_end_frame (Aurora's FIFO is
// drained, so its copy-texture table holds the copy, and its worker has the frame queued; with the
// async end of frame, Aurora patch 0018, after aurora_frame_sync), reads
// the copy texture back on the render worker behind the frame, box-filters it from the EFB's
// scale down to the copy's GX size and writes it into the buffer in GX's tiled layout, as the
// GameCube's copy left it (I8: 8x4 tiles, the luma; RGB565: 4x4 tiles, host-endian u16, which is
// how encode_s3tc reads it). The next frame the capture goes on. A readback that fails (logged)
// leaves mid grey, so the encoder never sees an out-of-range buffer.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc_internal.h"

#include "d/d_snap.h"

#include <aurora/aurora.h>
#include <lib/gfx/render_worker.hpp>
#include <lib/gx/gx.hpp>
#include <lib/webgpu/gpu.hpp>
#if defined(COS_SWITCH_DEKO3D)
// the deko3d NRO reads textures back through its renderer (switch/deko/dk_aurora.h)
#include "dk_aurora.h"
#endif

#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace {

namespace gpu = aurora::webgpu;

enum class State { Idle, Requested, Done };

State sState = State::Idle;
void* sDest = nullptr;
u32 sWidth = 0;
u32 sHeight = 0;
GXTexFmt sFormat = GX_TF_I8;
pc::CaptureStats sStats;

// The copy texture as 8-bit RGBA rows (srcWidth x srcHeight). Runs on the render worker.
bool readTexture(const aurora::gfx::TextureHandle& handle, std::vector<uint8_t>& rgba, uint32_t& srcWidth,
                 uint32_t& srcHeight) {
    const wgpu::TextureFormat format = handle->format;
    const bool bgra = format == wgpu::TextureFormat::BGRA8Unorm || format == wgpu::TextureFormat::BGRA8UnormSrgb;
    if (!bgra && format != wgpu::TextureFormat::RGBA8Unorm && format != wgpu::TextureFormat::RGBA8UnormSrgb) {
        pc::writef(STDERR_FILENO, "[cos] capture: copy texture format %u not supported\n", (unsigned int)format);
        return false;
    }
#if defined(COS_SWITCH_DEKO3D)
    {
        uint8_t* texels = nullptr;
        uint32_t w = 0, h = 0, f = 0;
        if (!aurora_switch_dk_read_texture(handle->texture.Get(), &texels, &w, &h, &f)) {
            pc::writef(STDERR_FILENO, "[cos] capture: readback did not complete (deko3d)\n");
            return false;
        }
        rgba.resize((size_t)w * h * 4);
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const uint8_t* p = texels + i * 4;
            uint8_t* o = rgba.data() + i * 4;
            o[0] = p[bgra ? 2 : 0];
            o[1] = p[1];
            o[2] = p[bgra ? 0 : 2];
            o[3] = p[3];
        }
        srcWidth = w;
        srcHeight = h;
        return true;
    }
#endif
    const uint32_t width = handle->size.width;
    const uint32_t height = handle->size.height;
    const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
    const uint64_t size = (uint64_t)bytesPerRow * height;
    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "switchwaker capture readback",
        .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
        .size = size,
    };
    wgpu::Buffer buffer = gpu::g_device.CreateBuffer(&bufferDescriptor);
    const wgpu::CommandEncoderDescriptor encoderDescriptor{.label = "switchwaker capture"};
    wgpu::CommandEncoder encoder = gpu::g_device.CreateCommandEncoder(&encoderDescriptor);
    const wgpu::TexelCopyTextureInfo src{
        .texture = handle->texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = wgpu::TextureAspect::All,
    };
    const wgpu::TexelCopyBufferInfo dst{
        .layout = wgpu::TexelCopyBufferLayout{.offset = 0, .bytesPerRow = bytesPerRow, .rowsPerImage = height},
        .buffer = buffer,
    };
    const wgpu::Extent3D extent{width, height, 1};
    encoder.CopyTextureToBuffer(&src, &dst, &extent);
    const wgpu::CommandBuffer commands = encoder.Finish();
    gpu::g_queue.Submit(1, &commands);

    bool mapped = false;
    const wgpu::Future future = buffer.MapAsync(
        wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::WaitAnyOnly,
        [&mapped](wgpu::MapAsyncStatus status, wgpu::StringView) { mapped = status == wgpu::MapAsyncStatus::Success; });
    const wgpu::WaitStatus wait = gpu::g_instance.WaitAny(future, 5'000'000'000ull);
    if (wait != wgpu::WaitStatus::Success || !mapped) {
        pc::writef(STDERR_FILENO, "[cos] capture: readback did not complete (wait %u)\n", (unsigned int)wait);
        return false;
    }
    const uint8_t* data = static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, size));
    if (data == nullptr) {
        buffer.Unmap();
        return false;
    }
    rgba.resize((size_t)width * height * 4);
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = data + (size_t)y * bytesPerRow;
        uint8_t* out = rgba.data() + (size_t)y * width * 4;
        for (uint32_t x = 0; x < width; x++) {
            out[x * 4 + 0] = row[x * 4 + (bgra ? 2 : 0)];
            out[x * 4 + 1] = row[x * 4 + 1];
            out[x * 4 + 2] = row[x * 4 + (bgra ? 0 : 2)];
            out[x * 4 + 3] = row[x * 4 + 3];
        }
    }
    buffer.Unmap();
    srcWidth = width;
    srcHeight = height;
    return true;
}

// The mean RGB of the source texels that GX texel (x, y) of a width x height copy covers.
void boxTexel(const std::vector<uint8_t>& rgba, uint32_t srcWidth, uint32_t srcHeight, u32 width, u32 height, u32 x,
              u32 y, uint32_t rgb[3]) {
    const uint32_t x0 = x * srcWidth / width, x1 = std::max(x0 + 1, (x + 1) * srcWidth / width);
    const uint32_t y0 = y * srcHeight / height, y1 = std::max(y0 + 1, (y + 1) * srcHeight / height);
    uint32_t sum[3] = {};
    for (uint32_t sy = y0; sy < y1; sy++) {
        const uint8_t* p = rgba.data() + ((size_t)sy * srcWidth + x0) * 4;
        for (uint32_t sx = x0; sx < x1; sx++, p += 4) {
            sum[0] += p[0];
            sum[1] += p[1];
            sum[2] += p[2];
        }
    }
    const uint32_t n = (x1 - x0) * (y1 - y0);
    for (int c = 0; c < 3; c++) {
        rgb[c] = (sum[c] + n / 2) / n;
    }
}

// GX's tiled layout of the copy, as encode_s3tc reads it. rgba null: mid grey.
void writeGx(const std::vector<uint8_t>* rgba, uint32_t srcWidth, uint32_t srcHeight) {
    uint8_t* dest = static_cast<uint8_t*>(sDest);
    for (u32 y = 0; y < sHeight; y++) {
        for (u32 x = 0; x < sWidth; x++) {
            uint32_t rgb[3] = {128, 128, 128};
            if (rgba != nullptr) {
                boxTexel(*rgba, srcWidth, srcHeight, sWidth, sHeight, x, y, rgb);
            }
            const uint32_t luma = (rgb[0] * 30 + rgb[1] * 59 + rgb[2] * 11) / 100;
            sStats.minLuma = std::min(sStats.minLuma, luma);
            sStats.maxLuma = std::max(sStats.maxLuma, luma);
            if (sFormat == GX_TF_I8) {
                // 8x4 tiles of 32 bytes. Aurora's I8 conversion already wrote the luma (16..235)
                // into every channel (lib/gfx/tex_copy_conv.cpp, intensity).
                const size_t offset = ((size_t)(y / 4) * (sWidth / 8) + x / 8) * 32 + (y % 4) * 8 + x % 8;
                dest[offset] = (uint8_t)std::clamp<uint32_t>(rgb[0], 16, 235);
            } else {
                // GX_TF_RGB565: 4x4 tiles of 32 bytes.
                const size_t offset = ((size_t)(y / 4) * (sWidth / 4) + x / 4) * 32 + (y % 4) * 8 + (x % 4) * 2;
                const u16 texel = (u16)((rgb[0] & 0xF8) << 8 | (rgb[1] & 0xFC) << 3 | rgb[2] >> 3);
                std::memcpy(dest + offset, &texel, sizeof(texel));
            }
        }
    }
}

} // namespace

bool pc_gph_capture_ready(void* dest, u32 width, u32 height, int format) {
    if (sState == State::Done && sDest == dest) {
        sState = State::Idle;
        return true;
    }
    if (format != GX_TF_I8 && format != GX_TF_RGB565) {
        pc::writef(STDERR_FILENO, "[cos] capture: format %u is not read back\n", (unsigned int)format);
        return true;
    }
    sState = State::Requested;
    sDest = dest;
    sWidth = width;
    sHeight = height;
    sFormat = (GXTexFmt)format;
    return false;
}

namespace {

// The copy texture GXCopyTex made for dest, as 8-bit RGBA rows at the EFB's scale. False (logged)
// without one or if the readback failed. The lookup is Aurora's (find_copy_texture, Aurora patch
// 0017): never read g_gxState here. On the Switch this unit is built by clang and Aurora by GCC,
// whose GXState differ (math.hpp's Vec4 uses GCC vector extensions): the copyTextures field was 80
// bytes off and the picto box crashed reading it (bug B38).
bool readCopy(const void* dest, const char* what, std::vector<uint8_t>& rgba, uint32_t& srcWidth,
              uint32_t& srcHeight) {
    // With the async end of frame (Aurora patch 0018, COS_ASYNC_END_FRAME) the GX worker may still be
    // ending the frame: wait for it, so the copy texture exists and the readback goes in behind the frame.
    aurora_frame_sync();
    const aurora::gfx::TextureHandle handle = aurora::gx::find_copy_texture(dest);
    if (!handle) {
        pc::writef(STDERR_FILENO, "[cos] %s: no copy texture for %p\n", what, dest);
        return false;
    }
    bool ok = false;
    aurora::gfx::render_worker::enqueue_work([&] { ok = readTexture(handle, rgba, srcWidth, srcHeight); });
    aurora::gfx::render_worker::synchronize();
    if (!ok) {
        pc::writef(STDERR_FILENO, "[cos] %s: readback failed\n", what);
    }
    return ok;
}

void captureReadBack() {
    if (sState != State::Requested) {
        return;
    }
    sState = State::Done;
    sStats.minLuma = 255;
    sStats.maxLuma = 0;
    std::vector<uint8_t> rgba;
    uint32_t srcWidth = 0, srcHeight = 0;
    if (!readCopy(sDest, "capture", rgba, srcWidth, srcHeight)) {
        pc::writef(STDERR_FILENO, "[cos] capture: the photo is grey\n");
        writeGx(nullptr, 0, 0);
        sStats.failed++;
        return;
    }
    writeGx(&rgba, srcWidth, srcHeight);
    sStats.readBack++;
    sStats.lastFormat = sFormat;
    pc::writef(STDERR_FILENO, "[cos] capture: %ux%u %s copy read back from %ux%u, luma %u..%u\n",
               (unsigned int)sWidth, (unsigned int)sHeight, sFormat == GX_TF_I8 ? "I8" : "RGB565",
               (unsigned int)srcWidth, (unsigned int)srcHeight, (unsigned int)sStats.minLuma,
               (unsigned int)sStats.maxLuma);
}

// pc_efb_peek_request's copy: its destination names Aurora's copy texture.
alignas(32) u8 sPeekKey[32];
void (*sPeekDone)() = nullptr;
u16 sPeekLeft = 0, sPeekTop = 0, sPeekWidth = 0, sPeekHeight = 0;
bool sPeekValid = false;
std::vector<uint8_t> sPeek;
uint32_t sPeekSrcWidth = 0, sPeekSrcHeight = 0;

void peekReadBack() {
    if (sPeekDone == nullptr) {
        return;
    }
    void (*done)() = sPeekDone;
    sPeekDone = nullptr;
    sPeekValid = readCopy(sPeekKey, "efb-peek", sPeek, sPeekSrcWidth, sPeekSrcHeight);
    done();
    sPeekValid = false;
}

} // namespace

void pc_efb_peek_request(u16 left, u16 top, u16 width, u16 height, void (*done)()) {
    if (sPeekDone != nullptr) {
        pc::writef(STDERR_FILENO, "[cos] efb-peek: a request is already pending; dropped\n");
        return;
    }
    sPeekLeft = left;
    sPeekTop = top;
    sPeekWidth = width;
    sPeekHeight = height;
    sPeekDone = done;
    GXSetTexCopySrc(left, top, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(sPeekKey, GX_FALSE);
    GXPixModeSync();
}

u32 pc_efb_peek_argb(u16 x, u16 y) {
    if (!sPeekValid || x < sPeekLeft || y < sPeekTop || x >= sPeekLeft + sPeekWidth || y >= sPeekTop + sPeekHeight) {
        return 0xFFFFFFFF;
    }
    // The texel under the centre of the logical pixel.
    const uint32_t sx = ((uint32_t)(x - sPeekLeft) * 2 + 1) * sPeekSrcWidth / (2u * sPeekWidth);
    const uint32_t sy = ((uint32_t)(y - sPeekTop) * 2 + 1) * sPeekSrcHeight / (2u * sPeekHeight);
    const uint8_t* p = sPeek.data() + ((size_t)sy * sPeekSrcWidth + sx) * 4;
    return (u32)p[3] << 24 | (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
}

void pc_snap_judged(int count, const dSnap_RegistObjElm* table, int result) {
    int seen = 0;
    char line[512];
    int len = 0;
    for (int col = 0; col < count; col++) {
        const dSnap_Obj& obj = table[col].m_obj;
        if (obj.mCapturedPixels == 0) {
            continue;
        }
        seen++;
        if (len < (int)sizeof(line) - 40) {
            len += snprintf(line + len, sizeof(line) - len, " %d:%d px %.2f", (int)obj.mPhotoNo,
                            (int)obj.mCapturedPixels, (double)obj.mCapturedRatio);
        }
    }
    line[len] = '\0';
    sStats.snapJudged++;
    sStats.snapSeen = seen;
    sStats.snapResult = result;
    pc::writef(STDERR_FILENO, "[cos] snap: %d registered, %d in the photo (photo:pixels ratio)%s; result %d%s\n",
               count, seen, line, result, sPeekValid ? "" : " (no readback: nothing seen)");
}

namespace pc {

void captureFrameEnd() {
    captureReadBack();
    peekReadBack();
}

const CaptureStats& captureStats() { return sStats; }

} // namespace pc
