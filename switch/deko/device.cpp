// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/backend.cpp (device, queue, swapchain, present, frame fences, debug callback,
// queue checks, the set-up cost and the statistics).
// The deko3d device, the frame's command recording and the present pass (dk.h, dk_aurora.h;
// docs/DEKO3D_MIGRATION_PLAN.md phases 2-3). Aurora's render worker opens a frame at Aurora's frame
// start (frame_open: the slot's fence, the descriptor sets, ImGui's texture uploads), records the
// frame's passes into it (switch/deko/aurora), submits along the way, and presents: the swapchain
// image cleared, the game's picture (the picture callback: Aurora's EFB resampled and fitted) or the
// test pattern with COS_DK_TEST_PATTERN=1, then ImGui (the options menu, the loading screen, the FPS
// panel), the fence, the submit and the present. One render thread records and submits.
#include <switch.h>
#include <unistd.h>

#include "dk.h"
#include "dk_aurora.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// the renderer's own shaders, compiled by uam at build time (switch/native/CMakeLists.txt, shaders/*.glsl)
#include "imgui_fsh_dksh.h"
#include "imgui_vsh_dksh.h"
#include "text_fsh_dksh.h"
#include "text_vsh_dksh.h"

extern "C" char* fake_heap_end;
// the game's host allocation scope (JKRHeap.cpp, native/src/pc/game_hooks/pc_jkr_heap.cpp)
void JKRPcBeginHostAlloc();              // libnx: the end of the heap malloc grows into (sbrk)
extern "C" void cos_switch_flush_logs(void);  // switch/native/source/cos_switch.cpp

namespace swdk {

Renderer R;
Conventions C;

namespace {

constexpr uint32_t kSwapImages = 3;
#ifdef COS_DK_DEBUG_LIB
constexpr bool kDebugLib = true;  // linked against libdeko3dd (COS_DK_DEBUG_LIB, switch/native/CMakeLists.txt)
#else
constexpr bool kDebugLib = false;
#endif

DkSwapchain g_swapchain = nullptr;
DkImage g_swapImages[kSwapImages];
ImageAlloc g_swapMem[kSwapImages];
DkImage g_depth;  // the test pattern's depth buffer (the game's targets come in phase 3)
ImageAlloc g_depthMem;
DkShader g_shaders[kShaderCount];
bool g_shaderOk[kShaderCount] = {};
int g_debugMessages = 0;
bool g_testPattern = false;
size_t g_heapAfterSetup = 0;

// the heap malloc has never grown into, in MiB (libnx's heap end against malloc's break): deko3d's
// memory blocks come from it, so the difference across the set-up is its cost (SwitchWakerHD core.cpp)
size_t heap_never_used_mib() {
    char* top = static_cast<char*>(sbrk(0));
    if (!fake_heap_end || !top || top == reinterpret_cast<char*>(-1)) return 0;
    return size_t(fake_heap_end - top) >> 20;
}

// deko3d's messages. Only the debug library (libdeko3dd, COS_DK_DEBUG_LIB=ON) calls this: its checks
// of every call, warnings (result DkResult_Success) and errors, after which it traps. The release
// library calls nothing on an error: it aborts with result 2359-xxxx.
void debug_message(void*, const char* context, DkResult result, const char* message) {
    if (result != DkResult_Success) {
        fatal("deko3d error in %s: result %d: %s (frame %llu)", context ? context : "?", int(result),
              message ? message : "", (unsigned long long)R.frame + 1);
    }
    const int n = g_debugMessages++;
    if (n < 200) {
        dklog("deko3d %s: %s", context ? context : "?", message ? message : "");
    } else if (n == 200) {
        dklog("deko3d: more messages, not logged");
    }
}

void load_builtin_shaders() {
    static const struct {
        const uint8_t* data;
        size_t size;
        const char* name;
    } kEmbedded[kShaderCount] = {
        {imgui_vsh_dksh, imgui_vsh_dksh_size, "imgui_vsh"},
        {imgui_fsh_dksh, imgui_fsh_dksh_size, "imgui_fsh"},
        {text_vsh_dksh, text_vsh_dksh_size, "text_vsh"},
        {text_fsh_dksh, text_fsh_dksh_size, "text_fsh"},
    };
    size_t bytes = 0;
    int ok = 0;
    for (int i = 0; i < kShaderCount; i++) {
        g_shaderOk[i] = code_load(g_shaders[i], kEmbedded[i].data, uint32_t(kEmbedded[i].size), kEmbedded[i].name);
        ok += g_shaderOk[i];
        bytes += kEmbedded[i].size;
    }
    dklog("embedded shaders (ImGui, text): %d of %d loaded (%zu bytes of DKSH)%s", ok, int(kShaderCount), bytes,
          ok == kShaderCount ? "" : ": the options menu and the loading screen cannot be drawn");
}

void init_image(DkImage& image, ImageAlloc& mem, DkImageFormat format, uint32_t flags, const char* what) {
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, R.device);
    m.flags = flags;
    m.format = format;
    m.dimensions[0] = R.width;
    m.dimensions[1] = R.height;
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &m);
    mem = image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
    dkImageInitialize(&image, &layout, mem.block, mem.offset);
    dklog("%s: %ux%u, %u KiB at image heap offset 0x%X", what, R.width, R.height, mem.size >> 10, mem.offset);
}

// ---- the window (the swapchain images): 1280x720 handheld, 1920x1080 docked (the TV's output), the
// swapchain recreated when the operation mode changes; the present pass fits the game's picture and ImGui
// into it (dk_present.cpp, overlay.cpp scale by R.width/R.height). SwitchWakerHD backend.cpp window_wanted.
// COS_DK_DOCKED_1080=0: 1280x720 always (the system scales it to the TV, the GL NRO's way).
// COS_DK_WINDOW=WxH (320x180 to 1920x1080): that size in both modes, for A/B tests in handheld.
struct WindowConfig {
    int mode = 1;  // 0 always 1280x720, 1 by the operation mode, 2 forced
    uint32_t w = 0, h = 0;
};
const WindowConfig& window_config() {
    static const WindowConfig c = [] {
        WindowConfig c;
        if (const char* e = getenv("COS_DK_WINDOW"); e && *e) {
            unsigned w = 0, h = 0;
            if (sscanf(e, "%ux%u", &w, &h) == 2 && w >= 320 && h >= 180 && w <= 1920 && h <= 1080) {
                c.mode = 2;
                c.w = w;
                c.h = h;
            } else {
                dklog("COS_DK_WINDOW=%s ignored: WxH from 320x180 to 1920x1080", e);
            }
        }
        if (c.mode != 2 && !env_flag("COS_DK_DOCKED_1080", true)) c.mode = 0;
        if (c.mode == 2) {
            dklog("window: %ux%u in both modes (COS_DK_WINDOW)", c.w, c.h);
        } else if (c.mode == 1) {
            dklog("window: docked 1920x1080, handheld 1280x720, the swapchain recreated when the mode changes "
                  "(COS_DK_DOCKED_1080=0: 1280x720 always)");
        } else {
            dklog("window: 1280x720 in both modes (COS_DK_DOCKED_1080=0; docked the system scales it)");
        }
        return c;
    }();
    return c;
}
int g_opMode = -1;  // the operation mode the window was last sized for: 1 docked, 0 handheld, -1 not yet
uint64_t g_windowResizes = 0;

// the window size for the current operation mode (libnx keeps appletGetOperationMode up to date from
// AppletMessage_OperationModeChanged in the main thread's appletMainLoop)
void window_wanted(uint32_t& w, uint32_t& h) {
    const WindowConfig& c = window_config();
    const int docked = appletGetOperationMode() == AppletOperationMode_Console ? 1 : 0;
    if (docked != g_opMode) {
        dklog("operation mode: %s", docked ? "docked (TV)" : "handheld");
        g_opMode = docked;
    }
    if (c.mode == 2) {
        w = c.w;
        h = c.h;
        return;
    }
    const bool big = c.mode == 1 && docked == 1;
    w = big ? 1920 : 1280;
    h = big ? 1080 : 720;
}

void init_swapchain() {
    const DkImage* images[kSwapImages];
    for (uint32_t i = 0; i < kSwapImages; i++) {
        char what[48];
        snprintf(what, sizeof what, "swapchain image %u (RGBA8)", i);
        init_image(g_swapImages[i], g_swapMem[i], DkImageFormat_RGBA8_Unorm,
                   DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression, what);
        images[i] = &g_swapImages[i];
    }
    init_image(g_depth, g_depthMem, DkImageFormat_Z24S8, DkImageFlags_UsageRender | DkImageFlags_HwCompression,
               "test pattern depth buffer (Z24S8)");
    DkSwapchainMaker m;
    dkSwapchainMakerDefaults(&m, R.device, nwindowGetDefault(), images, kSwapImages);
    dklog("creating the swapchain on the default window");
    log_flush();
    g_swapchain = dkSwapchainCreate(&m);
    dkSwapchainSetSwapInterval(g_swapchain, 1);
    u32 nw = 0, nh = 0;
    nwindowGetDimensions(nwindowGetDefault(), &nw, &nh);
    dklog("swapchain: %u RGBA8 images of %ux%u on the default window (%ux%u), swap interval 1", kSwapImages,
          R.width, R.height, nw, nh);
}

// a new window size (the operation mode changed): the GPU finishes what was submitted (the previous
// presents into the old images), the old swapchain gives the window's buffers back, and images and a
// swapchain of the new size take their place (deko3d sets the window's dimensions from them). The old
// images' memory is freed once the GPU is done with this frame.
void resize_window(uint32_t w, uint32_t h, uint64_t frame) {
    const uint64_t t0 = now_ns();
    const uint32_t ow = R.width, oh = R.height;
    check_queue("recreating the swapchain");
    dkQueueWaitIdle(R.queue);
    const uint64_t t1 = now_ns();
    dkSwapchainDestroy(g_swapchain);
    g_swapchain = nullptr;
    for (uint32_t i = 0; i < kSwapImages; i++) image_free_later(g_swapMem[i]);
    image_free_later(g_depthMem);
    R.width = w;
    R.height = h;
    init_swapchain();
    g_windowResizes++;
    dklog("frame %llu: window %ux%u -> %ux%u: swapchain recreated in %.1f ms (GPU idle wait %.1f ms)",
          (unsigned long long)frame, ow, oh, w, h, double(now_ns() - t0) / 1e6, double(t1 - t0) / 1e6);
}

// ---- statistics every 30 s: presents and where their time went, submits, command and stream memory per
// frame, the image heap, blocks, shader code, descriptors, the heap
struct Times {
    uint64_t presents = 0, fenceNs = 0, acquireNs = 0, recordNs = 0, submitNs = 0, maxNs = 0;
    uint64_t start = 0;
} g_times;
SubmitStats g_submits;
uint64_t g_submitsAtStats = 0;

void frame_stats(uint64_t presentNs) {
    g_times.presents++;
    g_times.maxNs = presentNs > g_times.maxNs ? presentNs : g_times.maxNs;
    const uint64_t now = now_ns();
    if (g_times.start == 0) g_times.start = now;
    if (now - g_times.start < 30'000'000'000ull) return;
    const double n = double(g_times.presents);
    const MemoryStats m = memory_stats_take();
    const DescriptorStats d = descriptor_stats();
    dklog("frames %llu-%llu: %.1f presents/s; per present: frame fence %.2f ms, acquire %.2f ms, record %.2f ms, "
          "submit+present %.2f ms, longest %.2f ms, %.1f submits; command memory max %llu KiB/frame (%llu "
          "overflows), stream max %llu KiB/frame (%llu full); image heap %llu KiB in %llu chunks, blocks %llu KiB, "
          "shader code %llu KiB; descriptors: %u images, %u samplers in use, written %llu images, %llu samplers "
          "(%llu sampler evictions); ImGui textures %u; window %ux%u (%llu resizes); deko3d messages %d; heap never "
          "used %zu MiB",
          (unsigned long long)(R.frame - g_times.presents + 1), (unsigned long long)R.frame,
          n * 1e9 / double(now - g_times.start), double(g_times.fenceNs) / n / 1e6, double(g_times.acquireNs) / n / 1e6,
          double(g_times.recordNs) / n / 1e6, double(g_times.submitNs) / n / 1e6, double(g_times.maxNs) / 1e6,
          double(g_submits.submits - g_submitsAtStats) / n, (unsigned long long)(m.cmdBytesMax >> 10),
          (unsigned long long)m.cmdOverflows, (unsigned long long)(m.streamBytesMax >> 10),
          (unsigned long long)m.streamFull, (unsigned long long)(m.imageBytes >> 10),
          (unsigned long long)m.imageChunks, (unsigned long long)(m.blockBytes >> 10),
          (unsigned long long)(m.codeBytes >> 10), d.imagesUsed, d.samplersUsed, (unsigned long long)d.imageWrites,
          (unsigned long long)d.samplerWrites, (unsigned long long)d.samplerEvictions, overlay_textures(),
          R.width, R.height, (unsigned long long)g_windowResizes, g_debugMessages, heap_never_used_mib());
    g_submitsAtStats = g_submits.submits;
    g_times = Times{};
    g_times.start = now;
}

// a 1x1 black texture in kEmptyImage: what a draw samples where its view is missing (the first frame)
bool g_emptyReady = false;
DkImage g_empty;
ImageAlloc g_emptyMem;
void empty_image_init() {
    if (g_emptyReady) return;
    DkImageLayoutMaker m;
    dkImageLayoutMakerDefaults(&m, R.device);
    m.format = DkImageFormat_RGBA8_Unorm;
    m.dimensions[0] = 1;
    m.dimensions[1] = 1;
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &m);
    StreamAlloc s = stream_alloc(4, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    if (!s) return;
    g_emptyMem = image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
    dkImageInitialize(&g_empty, &layout, g_emptyMem.block, g_emptyMem.offset);
    memset(s.cpu, 0, 4);
    static_cast<uint8_t*>(s.cpu)[3] = 0xFF;
    DkImageView view;
    dkImageViewDefaults(&view, &g_empty);
    const DkCopyBuf src = {s.gpu, 0, 0};
    const DkImageRect r = {0, 0, 0, 1, 1, 1};
    dkCmdBufCopyBufferToImage(R.cmd, &src, &view, &r, 0);
    dkCmdBufBarrier(R.cmd, DkBarrier_Full, DkInvalidateFlags_Image);
    write_image_descriptor(kEmptyImage, g_empty);
    g_emptyReady = true;
}

uint64_t g_frameOpenNs = 0;
PictureFn g_picture = nullptr;
void* g_pictureUser = nullptr;

// the commands recorded so far to the GPU (the fences they signal are written under fence_mutex)
void submit_list() {
    check_queue("a submit");
    const DkCmdList list = dkCmdBufFinishList(R.cmd);
    std::lock_guard<std::mutex> lock(fence_mutex());
    dkQueueSubmitCommands(R.queue, list);
    g_submits.submits++;
}

}  // namespace

void frame_open() {
    if (R.frameOpen) return;
    const uint64_t frame = R.frame + 1;
    const uint64_t t0 = now_ns();
    check_queue("the frame fence");
    frame_begin(frame);
    g_times.fenceNs += now_ns() - t0;
    g_frameOpenNs = now_ns();
    descriptors_frame_start();
    empty_image_init();
    overlay_upload_pending();
    if (g_testPattern) pattern_init();
    shaders_frame_start();
    R.frameOpen = true;
}

void submit(const char* why) {
    (void)why;
    if (!R.frameOpen) return;
    submit_list();
}

SubmitStats submit_stats() { return g_submits; }

void frame_present(const ImDrawData* ui, PictureFn picture, void* user) {
    frame_open();
    const uint64_t frame = R.frame + 1;
    {
        uint32_t ww, wh;
        window_wanted(ww, wh);
        if (ww != R.width || wh != R.height) resize_window(ww, wh, frame);
    }
    const uint64_t t1 = now_ns();
    int slot;
    {
        check_queue("acquiring a swapchain image");
        slot = dkQueueAcquireImage(R.queue, g_swapchain);
    }
    const uint64_t t2 = now_ns();
    if (picture && !g_testPattern) picture(user, 0);
    DkImageView color, depth;
    dkImageViewDefaults(&color, &g_swapImages[slot]);
    dkImageViewDefaults(&depth, &g_depth);
    const DkImageView* colors[] = {&color};
    dkCmdBufBindRenderTargets(R.cmd, colors, 1, &depth);
    static const DkViewportSwizzle kIdentity = {DkSwizzle_PositiveX, DkSwizzle_PositiveY, DkSwizzle_PositiveZ,
                                                DkSwizzle_PositiveW};
    dkCmdBufSetViewportSwizzles(R.cmd, 0, &kIdentity, 1);  // (the game's passes may have flipped y: C.flipY)
    set_view(0, 0, R.width, R.height);
    dkCmdBufClearColorFloat(R.cmd, 0, DkColorMask_RGBA, 0.0f, 0.0f, 0.0f, 1.0f);
    dkCmdBufClearDepthStencil(R.cmd, true, 1.0f, 0xFF, 0);
    if (g_testPattern) {
        pattern_draw();
    } else if (picture) {
        picture(user, 1);
        dkCmdBufSetViewportSwizzles(R.cmd, 0, &kIdentity, 1);  // (C.flipPresent)
        set_view(0, 0, R.width, R.height);
    }
    overlay_draw(ui);
    if (picture && !g_testPattern) picture(user, 2);  // (after ImGui: its GPU time)
    frame_end();
    const uint64_t t3 = now_ns();
    submit_list();
    check_queue("the present");
    dkQueuePresentImage(R.queue, g_swapchain, slot);
    const uint64_t t4 = now_ns();
    R.frame = frame;
    R.frameOpen = false;
    g_submits.frames++;
    if (frame == 1) {
        dklog("first present: acquire %.2f ms, record %.2f ms, submit+present %.2f ms; heap never used %zu MiB",
              double(t2 - t1) / 1e6, double(t3 - t2) / 1e6, double(t4 - t3) / 1e6, heap_never_used_mib());
    }
    g_times.acquireNs += t2 - t1;
    g_times.recordNs += (t1 - g_frameOpenNs) + (t3 - t2);
    g_times.submitNs += t4 - t3;
    frame_stats(t4 - g_frameOpenNs);
}

void set_picture(PictureFn picture, void* user) {
    g_picture = picture;
    g_pictureUser = user;
}

bool env_flag(const char* name, bool def) {
    const char* e = getenv(name);
    if (!e || !*e) return def;
    return *e != '0';
}

long env_long(const char* name, long def) {
    const char* e = getenv(name);
    if (!e || !*e) return def;
    char* end = nullptr;
    const long v = strtol(e, &end, 0);
    return end && end != e ? v : def;
}

void dklog(const char* format, ...) {
    char line[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (n < 0) return;
    fprintf(stderr, "[dk] %s\n", line);
}

void log_flush() { cos_switch_flush_logs(); }

void host_alloc_thread() {
    // The global operator new gives a thread's allocations to the game's current JKRHeap unless the
    // thread is in a host allocation scope: the render worker's and the uam worker's strings, maps
    // and vectors then took blocks from a game heap (NULL when it was full: a crash in the GPU
    // groups' marker names in the Tower of the Gods; dangling when the game freed it). These
    // threads run no game code: host memory for the whole thread.
    static thread_local bool done = false;
    if (!done) {
        done = true;
        JKRPcBeginHostAlloc();
    }
}

void fatal(const char* format, ...) {
    char line[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    fprintf(stderr, "[dk] FATAL: %s\n", line);
    log_flush();
    abort();  // the crash report: backtrace, state, logs written out (switch/native/source/cos_switch.cpp)
}

void check_queue(const char* before) {
    if (!dkQueueIsInErrorState(R.queue)) return;
    fatal("the deko3d queue is in an error state (a GPU fault) before %s of frame %llu; deko3d messages %d", before,
          (unsigned long long)R.frame + 1, g_debugMessages);
}

const DkShader* builtin_shader(ShaderId id) { return g_shaderOk[id] ? &g_shaders[id] : nullptr; }

void set_view(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    const DkViewport vp = {float(x), float(y), float(w), float(h), 0.0f, 1.0f};
    const DkScissor sc = {x, y, w, h};
    dkCmdBufSetViewports(R.cmd, 0, &vp, 1);
    dkCmdBufSetScissors(R.cmd, 0, &sc, 1);
}

void bind_2d_state(bool blend) {
    DkRasterizerState rs;
    dkRasterizerStateDefaults(&rs);
    rs.cullMode = DkFace_None;
    dkCmdBufBindRasterizerState(R.cmd, &rs);
    DkColorState cs;
    dkColorStateDefaults(&cs);
    dkColorStateSetBlendEnable(&cs, 0, blend);
    dkCmdBufBindColorState(R.cmd, &cs);
    DkColorWriteState cw;
    dkColorWriteStateDefaults(&cw);
    dkCmdBufBindColorWriteState(R.cmd, &cw);
    DkBlendState bs;
    dkBlendStateDefaults(&bs);
    dkBlendStateSetFactors(&bs, DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_One,
                           DkBlendFactor_InvSrcAlpha);
    dkCmdBufBindBlendStates(R.cmd, 0, &bs, 1);
    DkDepthStencilState ds;
    dkDepthStencilStateDefaults(&ds);
    ds.depthTestEnable = false;
    ds.depthWriteEnable = false;
    dkCmdBufBindDepthStencilState(R.cmd, &ds);
}

}  // namespace swdk

using namespace swdk;

extern "C" void aurora_switch_dk_init(uint32_t gxConfigVersion) {
    const uint64_t t0 = now_ns();
    const size_t heapBefore = heap_never_used_mib();
    const AppletType applet = appletGetAppletType();
    dklog("deko3d renderer (docs/DEKO3D_MIGRATION_PLAN.md phase 3): %s library; heap never used before the set-up "
          "%zu MiB",
          kDebugLib ? "debug (libdeko3dd: every call checked)" : "release", heapBefore);
    if (applet != AppletType_Application && applet != AppletType_SystemApplication) {
        dklog("WARNING: not in title mode: the deko3d set-up alone needs ~250 MiB, which an applet does not have");
    }
    DkDeviceMaker dm;
    dkDeviceMakerDefaults(&dm);
    dm.cbDebug = debug_message;
    // window origin top left, depth 0 to 1, clip-space y up: WebGPU's conventions (dk.h)
    dm.flags = DkDeviceFlags_OriginUpperLeft | DkDeviceFlags_DepthZeroToOne;
    dklog("creating the device");
    log_flush();
    R.device = dkDeviceCreate(&dm);
    dklog("device created: flags 0x%X (origin upper left, depth 0 to 1)", dm.flags);

    DkQueueMaker qm;
    dkQueueMakerDefaults(&qm, R.device);
    // zcull off unless COS_DK_ZCULL=1: SwitchWakerHD saw depth-biased decals lose whole 4x8-pixel tiles
    // with it, and the GL path's driver (Mesa nouveau) never enables it (plan section 7.5)
    const char* zcull = getenv("COS_DK_ZCULL");
    R.zcull = zcull && *zcull && *zcull != '0';
    qm.flags = DkQueueFlags_Graphics | DkQueueFlags_MediumPrio |
               (R.zcull ? DkQueueFlags_EnableZcull : DkQueueFlags_DisableZcull);
    qm.commandMemorySize = kQueueCommandMemory;
    qm.flushThreshold = qm.commandMemorySize / 8;
    dklog("creating the queue");
    log_flush();
    R.queue = dkQueueCreate(&qm);
    dklog("queue created: graphics, command memory %u KiB, zcull %s", qm.commandMemorySize >> 10,
          R.zcull ? "ON (COS_DK_ZCULL=1)" : "off (COS_DK_ZCULL=1 turns it on)");

    C.flipY = env_flag("COS_DK_FLIP_Y", false);
    C.flipFront = env_flag("COS_DK_FLIP_FRONT", false);
    C.flipTexture = env_flag("COS_DK_FLIP_TEXTURE", false);
    C.flipPresent = env_flag("COS_DK_FLIP_PRESENT", false);
    dklog("conventions: clip-space y %s (COS_DK_FLIP_Y=%d), Aurora's front face %s (COS_DK_FLIP_FRONT=%d), "
          "texture uploads %s (COS_DK_FLIP_TEXTURE=%d), present %s (COS_DK_FLIP_PRESENT=%d); the plan's defaults "
          "are all 0 (WebGPU's conventions as they are)",
          C.flipY ? "NEGATED by a viewport swizzle" : "as WebGPU's (up)", int(C.flipY),
          C.flipFront ? "INVERTED" : "as it is (CW -> DkFrontFace_CW)", int(C.flipFront),
          C.flipTexture ? "FLIPPED (bottom row first)" : "row 0 first", int(C.flipTexture),
          C.flipPresent ? "FLIPPED vertically" : "upright", int(C.flipPresent));

    memory_init();
    load_builtin_shaders();
    window_wanted(R.width, R.height);  // the first swapchain at the current mode's size
    init_swapchain();
    g_testPattern = pattern_enabled();
    dklog("present: %s", g_testPattern ? "the TEST PATTERN (COS_DK_TEST_PATTERN=1) under ImGui, instead of the game"
                                       : "the game's picture (the EFB resampled, fitted) under ImGui; "
                                         "COS_DK_TEST_PATTERN=1 shows the test pattern instead");
    shader_cache_init(gxConfigVersion);
    shaders_init(gxConfigVersion);
    g_heapAfterSetup = heap_never_used_mib();
    dklog("set-up cost: %zu MiB of heap (never used: %zu MiB before, %zu MiB after), %.0f ms",
          heapBefore > g_heapAfterSetup ? heapBefore - g_heapAfterSetup : 0, heapBefore, g_heapAfterSetup,
          double(now_ns() - t0) / 1e6);
    log_flush();
}

extern "C" void aurora_switch_dk_present(const ImDrawData* ui) {
    host_alloc_thread();
    frame_present(ui, g_picture, g_pictureUser);
}

extern "C" void aurora_switch_dk_shutdown(void) {
    if (R.queue == nullptr) return;
    dkQueueWaitIdle(R.queue);
    if (g_swapchain != nullptr) {
        dkSwapchainDestroy(g_swapchain);  // gives the window's buffers back (nwindowReleaseBuffers)
        g_swapchain = nullptr;
    }
    dklog("shut down after %llu frames", (unsigned long long)R.frame);
}
