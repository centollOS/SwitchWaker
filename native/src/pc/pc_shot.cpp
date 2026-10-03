// Screenshots of the presented frame (COS_SHOT, COS_SHOT_EVERY; docs/NATIVE_PORT_PHASE4_6.md,
// phase 6 log): what the window shows, read back from the GPU, so a run needs no macOS Screen
// Recording permission to keep an image of what it drew.
//
// - COS_SHOT=<frame>[,<frame>...] and/or COS_SHOT_EVERY=<n> name game frames (the numbering of
//   pc_frame_count and COS_TRACE=frame: frame 1 is the first that pc_frame_end closes). Each is
//   saved as shot-<frame, 6 digits>.png in COS_SHOT_DIR, else COS_RUN_DIR (run.sh's run
//   directory), else the current directory. Without either variable this file does nothing.
// - The image is Aurora's present source (lib/webgpu/gpu.cpp present_source: the EFB render
//   texture, its resolved copy under MSAA) at its own size: what the present pass scales into the
//   window, without the letterboxing and without the ImGui overlay.
// - Threading: Aurora encodes and submits a frame on its render worker, in queue order
//   (lib/gfx/render_worker.cpp). shotFrameEnd runs on the game thread right after
//   aurora_end_frame queued the frame, and queues the readback behind it: on the worker, the frame
//   is already submitted and the next one not yet begun. The readback copies the texture into a
//   buffer (rows padded to 256 bytes), submits, waits for the map (Instance::WaitAny, as
//   gpu.cpp waits for the adapter) and writes the PNG there. The game thread then waits for the
//   worker (render_worker::synchronize), so a shot taken right before an exit is on disk; a
//   frame with a shot runs late, which only matters to a run that measures pacing.
// - The PNG is written by hand: 8-bit RGB, deflate "stored" blocks (no compression), so no
//   library is needed; a 640x480 frame is about 0.9 MB.
#include "pc_internal.h"

#include <lib/gfx/render_worker.hpp>
#include <lib/webgpu/gpu.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

namespace gpu = aurora::webgpu;

std::vector<unsigned int> sShotFrames; // sorted, COS_SHOT
unsigned int sShotEvery = 0;           // COS_SHOT_EVERY, 0 = off
const char* sShotDir = nullptr;        // COS_SHOT_DIR, else COS_RUN_DIR, else "."
bool sShotOn = false;

// --- PNG (RFC 2083), stored deflate blocks -----------------------------------------------------

uint32_t sCrcTable[256];

void initCrcTable() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        sCrcTable[n] = c;
    }
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc = sCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

void putBe32(std::string& out, uint32_t v) {
    out.push_back((char)(v >> 24));
    out.push_back((char)(v >> 16));
    out.push_back((char)(v >> 8));
    out.push_back((char)v);
}

void putChunk(std::string& out, const char* type, const std::string& data) {
    putBe32(out, (uint32_t)data.size());
    const size_t start = out.size();
    out.append(type, 4);
    out.append(data);
    const uint32_t crc = crc32Update(0xFFFFFFFFu, (const uint8_t*)out.data() + start, out.size() - start);
    putBe32(out, crc ^ 0xFFFFFFFFu);
}

// rgb: width * height * 3 bytes, top row first.
std::string encodePng(const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    // Raw scanlines: filter byte 0 (None), then the row.
    std::string raw;
    const size_t rowBytes = (size_t)width * 3;
    raw.reserve((rowBytes + 1) * height);
    for (uint32_t y = 0; y < height; y++) {
        raw.push_back('\0');
        raw.append((const char*)rgb.data() + y * rowBytes, rowBytes);
    }

    // zlib stream: header, stored blocks of up to 65535 bytes, Adler-32.
    std::string z;
    z.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    z.push_back((char)0x78);
    z.push_back((char)0x01);
    size_t pos = 0;
    do {
        const size_t len = std::min<size_t>(raw.size() - pos, 65535);
        const bool last = pos + len == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((char)(len & 0xFF));
        z.push_back((char)(len >> 8));
        z.push_back((char)(~len & 0xFF));
        z.push_back((char)((~len >> 8) & 0xFF));
        z.append(raw, pos, len);
        pos += len;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    putBe32(z, (b << 16) | a);

    std::string ihdr;
    putBe32(ihdr, width);
    putBe32(ihdr, height);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // colour type: RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace

    std::string png("\x89PNG\r\n\x1a\n", 8);
    putChunk(png, "IHDR", ihdr);
    putChunk(png, "IDAT", z);
    putChunk(png, "IEND", std::string());
    return png;
}

// --- readback (render worker) ------------------------------------------------------------------

// One texel of the present source as 8-bit RGB; false for a format this does not convert.
bool texelToRgb(wgpu::TextureFormat format, const uint8_t* p, uint8_t* rgb) {
    switch (format) {
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
        rgb[0] = p[2];
        rgb[1] = p[1];
        rgb[2] = p[0];
        return true;
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
        rgb[0] = p[0];
        rgb[1] = p[1];
        rgb[2] = p[2];
        return true;
    case wgpu::TextureFormat::RGB10A2Unorm: {
        uint32_t v;
        memcpy(&v, p, 4);
        rgb[0] = (uint8_t)(((v >> 0) & 0x3FF) >> 2);
        rgb[1] = (uint8_t)(((v >> 10) & 0x3FF) >> 2);
        rgb[2] = (uint8_t)(((v >> 20) & 0x3FF) >> 2);
        return true;
    }
    default:
        return false;
    }
}

void writeShot(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    char path[1024];
    int len = snprintf(path, sizeof(path), "%s/shot-%06u.png", sShotDir, frame);
    if (len < 0 || len >= (int)sizeof(path)) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: path too long\n", frame);
        return;
    }
    const std::string png = encodePng(rgb, width, height);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: cannot create %s (errno %d)\n", frame, path, errno);
        return;
    }
    size_t done = 0;
    while (done < png.size()) {
        ssize_t n = write(fd, png.data() + done, png.size() - done);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            writef(STDERR_FILENO, "[cos] shot: frame %u: write to %s failed (errno %d)\n", frame, path, errno);
            break;
        }
        done += (size_t)n;
    }
    close(fd);
    writef(STDERR_FILENO, "[cos] shot: frame %u: %ux%u -> %s\n", frame, (unsigned int)width,
           (unsigned int)height, path);
}

// Runs on the render worker, after the frame's submit and present.
void readBack(unsigned int frame) {
    const gpu::TextureWithSampler& source = gpu::present_source();
    const uint32_t width = source.size.width;
    const uint32_t height = source.size.height;
    if (!source.texture || width == 0 || height == 0) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: no present source\n", frame);
        return;
    }
    uint8_t probe[3];
    const uint8_t zero[4] = {};
    if (!texelToRgb(source.format, zero, probe)) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: present source format %u not supported\n", frame,
               (unsigned int)source.format);
        return;
    }

    const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
    const uint64_t size = (uint64_t)bytesPerRow * height;
    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "centollos shot readback",
        .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
        .size = size,
    };
    wgpu::Buffer buffer = gpu::g_device.CreateBuffer(&bufferDescriptor);

    const wgpu::CommandEncoderDescriptor encoderDescriptor{.label = "centollos shot"};
    wgpu::CommandEncoder encoder = gpu::g_device.CreateCommandEncoder(&encoderDescriptor);
    const wgpu::TexelCopyTextureInfo src{
        .texture = source.texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = wgpu::TextureAspect::All,
    };
    const wgpu::TexelCopyBufferInfo dst{
        .layout =
            wgpu::TexelCopyBufferLayout{
                .offset = 0,
                .bytesPerRow = bytesPerRow,
                .rowsPerImage = height,
            },
        .buffer = buffer,
    };
    const wgpu::Extent3D extent{width, height, 1};
    encoder.CopyTextureToBuffer(&src, &dst, &extent);
    const wgpu::CommandBuffer commands = encoder.Finish();
    gpu::g_queue.Submit(1, &commands);

    bool mapped = false;
    const wgpu::Future future = buffer.MapAsync(
        wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::WaitAnyOnly,
        [&mapped](wgpu::MapAsyncStatus status, wgpu::StringView message) {
            mapped = status == wgpu::MapAsyncStatus::Success;
            if (!mapped) {
                writef(STDERR_FILENO, "[cos] shot: map failed (status %u): %.*s\n", (unsigned int)status,
                       (int)message.length, message.data != nullptr ? message.data : "");
            }
        });
    const wgpu::WaitStatus wait = gpu::g_instance.WaitAny(future, 5'000'000'000ull);
    if (wait != wgpu::WaitStatus::Success || !mapped) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: readback did not complete (wait %u)\n", frame,
               (unsigned int)wait);
        return;
    }

    const uint8_t* data = static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, size));
    if (data == nullptr) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: no mapped range\n", frame);
        buffer.Unmap();
        return;
    }
    std::vector<uint8_t> rgb((size_t)width * height * 3);
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = data + (size_t)y * bytesPerRow;
        uint8_t* out = rgb.data() + (size_t)y * width * 3;
        for (uint32_t x = 0; x < width; x++) {
            texelToRgb(source.format, row + x * 4, out + x * 3);
        }
    }
    buffer.Unmap();
    writeShot(frame, rgb, width, height);
}

bool wanted(unsigned int frame) {
    if (sShotEvery != 0 && frame % sShotEvery == 0) {
        return true;
    }
    return std::binary_search(sShotFrames.begin(), sShotFrames.end(), frame);
}

} // namespace

void loadShots() {
    const char* list = getenv("COS_SHOT");
    if (list != nullptr && list[0] != '\0') {
        const char* p = list;
        while (*p != '\0') {
            char* end = nullptr;
            errno = 0;
            unsigned long n = strtoul(p, &end, 10);
            if (errno != 0 || end == p || n == 0 || n > 0xFFFFFFFFul || (*end != ',' && *end != '\0')) {
                writef(STDERR_FILENO, "[cos] COS_SHOT=\"%s\" is not a list of frames (e.g. 60,300)\n", list);
                pc_exit(PC_EXIT_USAGE);
            }
            sShotFrames.push_back((unsigned int)n);
            p = *end == ',' ? end + 1 : end;
        }
        std::sort(sShotFrames.begin(), sShotFrames.end());
    }
    const char* every = getenv("COS_SHOT_EVERY");
    if (every != nullptr && every[0] != '\0') {
        char* end = nullptr;
        errno = 0;
        unsigned long n = strtoul(every, &end, 10);
        if (errno != 0 || end == every || *end != '\0' || n == 0 || n > 0xFFFFFFFFul) {
            writef(STDERR_FILENO, "[cos] COS_SHOT_EVERY=\"%s\" is not a frame count\n", every);
            pc_exit(PC_EXIT_USAGE);
        }
        sShotEvery = (unsigned int)n;
    }
    sShotOn = !sShotFrames.empty() || sShotEvery != 0;
    if (!sShotOn) {
        return;
    }
    const char* dir = getenv("COS_SHOT_DIR");
    sShotDir = (dir != nullptr && dir[0] != '\0') ? dir : gConfig.runDir != nullptr ? gConfig.runDir : ".";
    initCrcTable();
    writef(STDERR_FILENO, "[cos] shot: %zu frame(s) listed, every %u, into %s\n", sShotFrames.size(),
           sShotEvery, sShotDir);
}

void shotFrameEnd(unsigned int frame) {
    if (!sShotOn || !wanted(frame)) {
        return;
    }
    aurora::gfx::render_worker::enqueue_work([frame] { readBack(frame); });
    aurora::gfx::render_worker::synchronize();
}

} // namespace pc
