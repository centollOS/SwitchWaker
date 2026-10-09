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
//   window, without the letterboxing and without the ImGui overlay (COS_SHOT_IMGUI=1 draws the
//   frame's ImGui windows over it on the CPU: the FPS overlay, the options menu).
// - Threading: Aurora encodes and submits a frame on its render worker, in queue order
//   (lib/gfx/render_worker.cpp). shotFrameEnd runs on the game thread right after
//   aurora_end_frame queued the frame, and queues the readback behind it: on the worker, the frame
//   is already submitted and the next one not yet begun. With the async end of frame (Aurora patch
//   0018) Aurora's GX worker queues the frame; aurora_frame_sync waits for that first. The readback copies the texture into a
//   buffer (rows padded to 256 bytes), submits, waits for the map (Instance::WaitAny, as
//   gpu.cpp waits for the adapter) and writes the PNG there. The game thread then waits for the
//   worker (render_worker::synchronize), so a shot taken right before an exit is on disk; a
//   frame with a shot runs late, which only matters to a run that measures pacing.
// - Every draw is in the shot: with shots on, COS_SYNC_PIPELINES defaults to on (pc_harness.cpp)
//   and Aurora compiles a pipeline before its first draw (Aurora patch 0005) instead of skipping
//   the draw while the pipeline compiles, which made shots depend on machine load (render audit
//   A3: Orca's message text and most of the HUD missing in parallel runs).
// - The PNG is written by hand: 8-bit RGB, deflate "stored" blocks (no compression), so no
//   library is needed; a 640x480 frame is about 0.9 MB.
#include "pc_internal.h"

#include <aurora/aurora.h>
#include <lib/gfx/render_worker.hpp>
#include <lib/webgpu/gpu.hpp>
#if defined(COS_SWITCH_DEKO3D)
// the deko3d NRO reads textures back through its renderer (switch/deko/dk_aurora.h): Dawn's device
// there is the Null one
#include "dk_aurora.h"
#endif

#include <imgui.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <memory>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
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

bool writePng(const char* path, unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height);

void writeShot(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    char path[1024];
    int len = snprintf(path, sizeof(path), "%s/shot-%06u.png", sShotDir, frame);
    if (len < 0 || len >= (int)sizeof(path)) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: path too long\n", frame);
        return;
    }
    writePng(path, frame, rgb, width, height);
}

bool writePng(const char* path, unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    const std::string png = encodePng(rgb, width, height);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: cannot create %s (errno %d)\n", frame, path, errno);
        return false;
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
    return done == png.size();
}

// Runs on the render worker, after the frame's submit and present: the present source as 8-bit
// RGB rows into rgb. False (logged) if it cannot be read.
bool readPixels(unsigned int frame, std::vector<uint8_t>& rgb, uint32_t& outWidth, uint32_t& outHeight) {
    const gpu::TextureWithSampler& source = gpu::present_source();
    const uint32_t width = source.size.width;
    const uint32_t height = source.size.height;
    if (!source.texture || width == 0 || height == 0) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: no present source\n", frame);
        return false;
    }
    uint8_t probe[3];
    const uint8_t zero[4] = {};
    if (!texelToRgb(source.format, zero, probe)) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: present source format %u not supported\n", frame,
               (unsigned int)source.format);
        return false;
    }
#if defined(COS_SWITCH_DEKO3D)
    {
        uint8_t* texels = nullptr;
        uint32_t w = 0, h = 0, format = 0;
        if (!aurora_switch_dk_read_texture(source.texture.Get(), &texels, &w, &h, &format)) {
            writef(STDERR_FILENO, "[cos] shot: frame %u: readback did not complete (deko3d)\n", frame);
            return false;
        }
        rgb.assign((size_t)w * h * 3, 0);
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                texelToRgb((wgpu::TextureFormat)format, texels + ((size_t)y * w + x) * 4, rgb.data() + ((size_t)y * w + x) * 3);
            }
        }
        outWidth = w;
        outHeight = h;
        return true;
    }
#endif

    const uint32_t bytesPerRow = (width * 4 + 255) & ~255u;
    const uint64_t size = (uint64_t)bytesPerRow * height;
    const wgpu::BufferDescriptor bufferDescriptor{
        .label = "switchwaker shot readback",
        .usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
        .size = size,
    };
    wgpu::Buffer buffer = gpu::g_device.CreateBuffer(&bufferDescriptor);

    const wgpu::CommandEncoderDescriptor encoderDescriptor{.label = "switchwaker shot"};
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
        return false;
    }

    const uint8_t* data = static_cast<const uint8_t*>(buffer.GetConstMappedRange(0, size));
    if (data == nullptr) {
        writef(STDERR_FILENO, "[cos] shot: frame %u: no mapped range\n", frame);
        buffer.Unmap();
        return false;
    }
    rgb.assign((size_t)width * height * 3, 0);
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = data + (size_t)y * bytesPerRow;
        uint8_t* out = rgb.data() + (size_t)y * width * 3;
        for (uint32_t x = 0; x < width; x++) {
            texelToRgb(source.format, row + x * 4, out + x * 3);
        }
    }
    buffer.Unmap();
    outWidth = width;
    outHeight = height;
    return true;
}

// COS_SHOT_IMGUI=1: the frame's ImGui draw data (the FPS overlay, the options menu), copied on the
// game thread right after aurora_end_frame rendered it, then drawn over the read-back image on the
// CPU: textured, vertex-coloured triangles, alpha-blended, clipped, the texture sampled nearest from
// the font atlas (the only texture these windows use). Coordinates go from ImGui's display (the
// window, in points) to the image (the EFB), so a letterboxed window is approximate. For evidence
// of the menus in tests; the real present composites on the GPU.
struct ImguiSnap {
    struct Vtx {
        float x, y, u, v;
        uint32_t col;
    };
    struct Cmd {
        float clip[4];
        uint32_t idxOffset, elemCount, vtxOffset;
        bool font;
    };
    std::vector<Vtx> vtx;
    std::vector<uint32_t> idx;
    std::vector<Cmd> cmds;
    float dispX = 0, dispY = 0, dispW = 0, dispH = 0;
    const unsigned char* atlas = nullptr;
    int atlasW = 0, atlasH = 0;
};

bool sShotImgui = false;

std::shared_ptr<ImguiSnap> snapImgui(bool force = false) {
    if ((!sShotImgui && !force) || ImGui::GetCurrentContext() == nullptr) {
        return nullptr;
    }
    ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr || !data->Valid || data->CmdListsCount == 0) {
        return nullptr;
    }
    auto snap = std::make_shared<ImguiSnap>();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &snap->atlasW, &snap->atlasH);
    snap->atlas = pixels;
    const ImTextureID fontTex = io.Fonts->TexID;
    snap->dispX = data->DisplayPos.x;
    snap->dispY = data->DisplayPos.y;
    snap->dispW = data->DisplaySize.x;
    snap->dispH = data->DisplaySize.y;
    for (int l = 0; l < data->CmdListsCount; l++) {
        const ImDrawList* list = data->CmdLists[l];
        const uint32_t base = (uint32_t)snap->vtx.size();
        const uint32_t idxBase = (uint32_t)snap->idx.size();
        for (const ImDrawVert& v : list->VtxBuffer) {
            snap->vtx.push_back({v.pos.x, v.pos.y, v.uv.x, v.uv.y, v.col});
        }
        for (ImDrawIdx i : list->IdxBuffer) {
            snap->idx.push_back(i);
        }
        for (const ImDrawCmd& c : list->CmdBuffer) {
            if (c.UserCallback != nullptr) {
                continue;
            }
            snap->cmds.push_back({{c.ClipRect.x, c.ClipRect.y, c.ClipRect.z, c.ClipRect.w},
                                  idxBase + c.IdxOffset, c.ElemCount, base + c.VtxOffset,
                                  c.GetTexID() == fontTex});
        }
    }
    return snap;
}

void compositeImgui(const ImguiSnap& s, std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    if (s.dispW <= 0 || s.dispH <= 0) {
        return;
    }
    const float sx = width / s.dispW, sy = height / s.dispH;
    for (const ImguiSnap::Cmd& c : s.cmds) {
        const int cx0 = std::max(0, (int)std::floor((c.clip[0] - s.dispX) * sx));
        const int cy0 = std::max(0, (int)std::floor((c.clip[1] - s.dispY) * sy));
        const int cx1 = std::min((int)width, (int)std::ceil((c.clip[2] - s.dispX) * sx));
        const int cy1 = std::min((int)height, (int)std::ceil((c.clip[3] - s.dispY) * sy));
        for (uint32_t t = 0; t + 2 < c.elemCount; t += 3) {
            const ImguiSnap::Vtx* v[3];
            for (int k = 0; k < 3; k++) {
                v[k] = &s.vtx[c.vtxOffset + s.idx[c.idxOffset + t + k]];
            }
            float px[3], py[3];
            for (int k = 0; k < 3; k++) {
                px[k] = (v[k]->x - s.dispX) * sx;
                py[k] = (v[k]->y - s.dispY) * sy;
            }
            const float area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0]);
            if (std::fabs(area) < 1e-6f) {
                continue;
            }
            const int x0 = std::max(cx0, (int)std::floor(std::min({px[0], px[1], px[2]})));
            const int x1 = std::min(cx1, (int)std::ceil(std::max({px[0], px[1], px[2]})));
            const int y0 = std::max(cy0, (int)std::floor(std::min({py[0], py[1], py[2]})));
            const int y1 = std::min(cy1, (int)std::ceil(std::max({py[0], py[1], py[2]})));
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    const float fx = x + 0.5f, fy = y + 0.5f;
                    const float w0 = ((px[1] - fx) * (py[2] - fy) - (px[2] - fx) * (py[1] - fy)) / area;
                    const float w1 = ((px[2] - fx) * (py[0] - fy) - (px[0] - fx) * (py[2] - fy)) / area;
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) {
                        continue;
                    }
                    float col[4];
                    for (int ch = 0; ch < 4; ch++) {
                        col[ch] = (w0 * ((v[0]->col >> (8 * ch)) & 0xFF) + w1 * ((v[1]->col >> (8 * ch)) & 0xFF) +
                                   w2 * ((v[2]->col >> (8 * ch)) & 0xFF)) /
                                  255.0f;
                    }
                    if (c.font && s.atlas != nullptr) {
                        const float u = w0 * v[0]->u + w1 * v[1]->u + w2 * v[2]->u;
                        const float vv = w0 * v[0]->v + w1 * v[1]->v + w2 * v[2]->v;
                        const int tx = std::min(s.atlasW - 1, std::max(0, (int)(u * s.atlasW)));
                        const int ty = std::min(s.atlasH - 1, std::max(0, (int)(vv * s.atlasH)));
                        const unsigned char* texel = s.atlas + ((size_t)ty * s.atlasW + tx) * 4;
                        for (int ch = 0; ch < 4; ch++) {
                            col[ch] *= texel[ch] / 255.0f;
                        }
                    }
                    uint8_t* dst = &rgb[((size_t)y * width + x) * 3];
                    for (int ch = 0; ch < 3; ch++) {
                        const float out = col[ch] * col[3] * 255.0f + dst[ch] * (1.0f - col[3]);
                        dst[ch] = (uint8_t)std::min(255.0f, std::max(0.0f, out + 0.5f));
                    }
                }
            }
        }
    }
}

void readBack(unsigned int frame, const std::shared_ptr<ImguiSnap>& imgui) {
    std::vector<uint8_t> rgb;
    uint32_t width = 0, height = 0;
    if (readPixels(frame, rgb, width, height)) {
        if (imgui != nullptr) {
            compositeImgui(*imgui, rgb, width, height);
        }
        writeShot(frame, rgb, width, height);
    }
}

bool wanted(unsigned int frame) {
    if (sShotEvery != 0 && frame % sShotEvery == 0) {
        return true;
    }
    return std::binary_search(sShotFrames.begin(), sShotFrames.end(), frame);
}

} // namespace

bool loadShots() {
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
    const char* withImgui = getenv("COS_SHOT_IMGUI");
    sShotImgui = withImgui != nullptr && strcmp(withImgui, "1") == 0;
    if (!sShotOn) {
        return false;
    }
    const char* dir = getenv("COS_SHOT_DIR");
    sShotDir = (dir != nullptr && dir[0] != '\0') ? dir : gConfig.runDir != nullptr ? gConfig.runDir : ".";
    initCrcTable();
    writef(STDERR_FILENO, "[cos] shot: %zu frame(s) listed, every %u, into %s\n", sShotFrames.size(),
           sShotEvery, sShotDir);
    return true;
}

void shotProbe(unsigned int frame,
               std::function<void(const std::vector<uint8_t>&, uint32_t, uint32_t)> check) {
    aurora_frame_sync(); // the frame on the render worker first (async end of frame, Aurora 0018)
    aurora::gfx::render_worker::enqueue_work([frame, &check] {
        std::vector<uint8_t> rgb;
        uint32_t width = 0, height = 0;
        if (readPixels(frame, rgb, width, height)) {
            check(rgb, width, height);
        }
    });
    aurora::gfx::render_worker::synchronize();
}

// ---- the Switch debug server's screenshot (switch/native/source/cos_debug.cpp, pc_debug_shot_request)
std::mutex sDebugShotMu;
std::string sDebugShotPath;   // where the next frame end writes it ("": nothing asked)
bool sDebugShotOverlay = false;
unsigned int sDebugShotAsked = 0, sDebugShotDone = 0, sDebugShotOk = 0;

void debugShotFrameEnd(unsigned int frame) {
    std::string path;
    bool overlay;
    unsigned int ticket;
    {
        std::lock_guard<std::mutex> lk(sDebugShotMu);
        if (sDebugShotPath.empty()) {
            return;
        }
        path.swap(sDebugShotPath);
        overlay = sDebugShotOverlay;
        ticket = sDebugShotAsked;
    }
    initCrcTable();
    std::shared_ptr<ImguiSnap> imgui = overlay ? snapImgui(true) : nullptr;
    bool ok = false;
    aurora_frame_sync(); // the frame on the render worker first (async end of frame, Aurora 0018)
    aurora::gfx::render_worker::enqueue_work([frame, imgui, &path, &ok] {
        std::vector<uint8_t> rgb;
        uint32_t width = 0, height = 0;
        if (readPixels(frame, rgb, width, height)) {
            if (imgui != nullptr) {
                compositeImgui(*imgui, rgb, width, height);
            }
            ok = writePng(path.c_str(), frame, rgb, width, height);
        }
    });
    aurora::gfx::render_worker::synchronize();
    std::lock_guard<std::mutex> lk(sDebugShotMu);
    sDebugShotDone = ticket;
    sDebugShotOk = ok ? ticket : sDebugShotOk;
}

void shotFrameEnd(unsigned int frame) {
    debugShotFrameEnd(frame);
    if (!sShotOn || !wanted(frame)) {
        return;
    }
    std::shared_ptr<ImguiSnap> imgui = snapImgui();
    aurora_frame_sync(); // the frame on the render worker first (async end of frame, Aurora 0018)
    aurora::gfx::render_worker::enqueue_work([frame, imgui] { readBack(frame, imgui); });
    aurora::gfx::render_worker::synchronize();
}

bool captureFrame(unsigned int frame, FrameSink sink, void* user) {
    bool ok = false;
    aurora_frame_sync(); // the frame on the render worker first (async end of frame, Aurora 0018)
    aurora::gfx::render_worker::enqueue_work([frame, sink, user, &ok] {
        std::vector<uint8_t> rgb;
        uint32_t width = 0, height = 0;
        ok = readPixels(frame, rgb, width, height);
        if (ok) {
            sink(frame, rgb, width, height, user);
        }
    });
    aurora::gfx::render_worker::synchronize();
    return ok;
}

void saveFramePng(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height) {
    if (sShotDir == nullptr) {
        const char* dir = getenv("COS_SHOT_DIR");
        sShotDir = (dir != nullptr && dir[0] != '\0') ? dir : gConfig.runDir != nullptr ? gConfig.runDir : ".";
        initCrcTable();
    }
    writeShot(frame, rgb, width, height);
}

} // namespace pc

// The debug server: the next frame end writes the presented picture to path as a PNG (overlay: with the
// frame's ImGui windows, the FPS panel and the options menu, drawn over it). Returns the request's number.
extern "C" unsigned int pc_debug_shot_request(const char* path, int overlay) {
    std::lock_guard<std::mutex> lk(pc::sDebugShotMu);
    pc::sDebugShotPath = path;
    pc::sDebugShotOverlay = overlay != 0;
    return ++pc::sDebugShotAsked;
}

// The number of the last request written (ok: and written well); 0 before any.
extern "C" unsigned int pc_debug_shot_done(int* ok) {
    std::lock_guard<std::mutex> lk(pc::sDebugShotMu);
    *ok = pc::sDebugShotOk == pc::sDebugShotDone && pc::sDebugShotDone != 0;
    return pc::sDebugShotDone;
}
