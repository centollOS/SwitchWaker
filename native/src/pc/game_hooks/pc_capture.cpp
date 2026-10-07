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
// drained, so its copy-texture table holds the copy, and its worker has the frame queued), reads
// the copy texture back on the render worker behind the frame, box-filters it from the EFB's
// scale down to the copy's GX size and writes it into the buffer in GX's tiled layout, as the
// GameCube's copy left it (I8: 8x4 tiles, the luma; RGB565: 4x4 tiles, host-endian u16, which is
// how encode_s3tc reads it). The next frame the capture goes on. A readback that fails (logged)
// leaves mid grey, so the encoder never sees an out-of-range buffer.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc_internal.h"

#include <lib/gfx/render_worker.hpp>
#include <lib/gx/gx.hpp>
#include <lib/webgpu/gpu.hpp>

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
    const uint32_t width = handle->size.width;
    const uint32_t height = handle->size.height;
    const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
    const uint64_t size = (uint64_t)bytesPerRow * height;
    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "centollos capture readback",
        .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
        .size = size,
    };
    wgpu::Buffer buffer = gpu::g_device.CreateBuffer(&bufferDescriptor);
    const wgpu::CommandEncoderDescriptor encoderDescriptor{.label = "centollos capture"};
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

namespace pc {

void captureFrameEnd() {
    if (sState != State::Requested) {
        return;
    }
    sState = State::Done;
    sStats.minLuma = 255;
    sStats.maxLuma = 0;
    const auto it = aurora::gx::g_gxState.copyTextures.find(sDest);
    if (it == aurora::gx::g_gxState.copyTextures.end() || !it->second.handle) {
        writef(STDERR_FILENO, "[cos] capture: no copy texture for %p; the photo is grey\n", sDest);
        writeGx(nullptr, 0, 0);
        sStats.failed++;
        return;
    }
    const aurora::gfx::TextureHandle handle = it->second.handle;
    bool ok = false;
    std::vector<uint8_t> rgba;
    uint32_t srcWidth = 0, srcHeight = 0;
    // Large host allocations on the worker, not in the game's JKR heaps.
    aurora::gfx::render_worker::enqueue_work([&] { ok = readTexture(handle, rgba, srcWidth, srcHeight); });
    aurora::gfx::render_worker::synchronize();
    if (!ok) {
        writef(STDERR_FILENO, "[cos] capture: readback failed; the photo is grey\n");
        writeGx(nullptr, 0, 0);
        sStats.failed++;
        return;
    }
    writeGx(&rgba, srcWidth, srcHeight);
    sStats.readBack++;
    sStats.lastFormat = sFormat;
    writef(STDERR_FILENO, "[cos] capture: %ux%u %s copy read back from %ux%u, luma %u..%u\n", (unsigned int)sWidth,
           (unsigned int)sHeight, sFormat == GX_TF_I8 ? "I8" : "RGB565", (unsigned int)srcWidth,
           (unsigned int)srcHeight, (unsigned int)sStats.minLuma, (unsigned int)sStats.maxLuma);
}

const CaptureStats& captureStats() { return sStats; }

} // namespace pc
