// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/overlay_dk.cpp, for this build's ImGui 1.91.9b (switch/native/CMakeLists.txt):
// no ImTextureData protocol, so the font atlas comes as one RGBA8 texture through
// aurora_switch_dk_imgui_texture (Aurora's imgui.cpp, Switch patch 0013) instead.
// Dear ImGui's draw data (the options menu, the loading screen, the FPS panel), drawn in the present
// pass over the picture. Textures live in the image heap with a descriptor each (image slots 1-63)
// and are drawn with the linear sampler; vertices, indices, the transform and the texture uploads go
// through the stream ring.
#include "dk.h"
#include "dk_aurora.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <mutex>
#include <vector>

namespace swdk {
namespace {

struct Texture {
    bool used = false;
    uint32_t width = 0, height = 0;
    DkImage image;
    ImageAlloc mem;
};
Texture g_tex[kOverlayFirstImage + kOverlayImages];  // indexed by image slot

struct Pending {
    uint32_t slot, width, height;
    std::vector<uint8_t> rgba;
};
std::mutex g_pendingMutex;
std::vector<Pending> g_pending;
uint32_t g_nextSlot = kOverlayFirstImage;  // under g_pendingMutex
uint32_t g_textures = 0;

}  // namespace

void overlay_upload_pending() {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        pending.swap(g_pending);
    }
    for (size_t i = 0; i < pending.size(); i++) {
        Pending& p = pending[i];
        Texture& t = g_tex[p.slot];
        if (!t.used || t.width != p.width || t.height != p.height) {
            if (t.used) image_free_later(t.mem);  // the GPU may still draw with it this frame
            DkImageLayoutMaker m;
            dkImageLayoutMakerDefaults(&m, R.device);
            m.format = DkImageFormat_RGBA8_Unorm;
            m.dimensions[0] = p.width;
            m.dimensions[1] = p.height;
            image_tile_size_fix(m, p.height);
            DkImageLayout layout;
            dkImageLayoutInitialize(&layout, &m);
            t.mem = image_alloc(uint32_t(dkImageLayoutGetSize(&layout)), dkImageLayoutGetAlignment(&layout));
            dkImageInitialize(&t.image, &layout, t.mem.block, t.mem.offset);
            t.used = true;
            t.width = p.width;
            t.height = p.height;
            write_image_descriptor(p.slot, t.image);
            g_textures++;
            dklog("ImGui texture %u: %ux%u", p.slot, p.width, p.height);
        }
        // the pixels through the stream ring, copied into the image by the GPU
        const uint32_t bytes = p.width * p.height * 4;
        StreamAlloc s = stream_alloc(bytes, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
        if (!s) {
            // logged by stream_alloc; tried again at the next present
            std::lock_guard<std::mutex> lock(g_pendingMutex);
            g_pending.insert(g_pending.end(), std::make_move_iterator(pending.begin() + long(i)),
                             std::make_move_iterator(pending.end()));
            break;
        }
        memcpy(s.cpu, p.rgba.data(), bytes);
        DkImageView view;
        dkImageViewDefaults(&view, &t.image);
        const DkCopyBuf src = {s.gpu, 0, 0};
        const DkImageRect rect = {0, 0, 0, p.width, p.height, 1};
        dkCmdBufCopyBufferToImage(R.cmd, &src, &view, &rect, 0);
        // the copy done before any draw samples the texture
        dkCmdBufBarrier(R.cmd, DkBarrier_Full, DkInvalidateFlags_Image | DkInvalidateFlags_Descriptors);
    }
}

uint32_t overlay_textures() { return g_textures; }

// Into the bound render target (the swapchain image), top-left origin like ImGui.
void overlay_draw(const ImDrawData* d) {
    if (!d || d->CmdListsCount == 0 || d->TotalVtxCount <= 0 || d->DisplaySize.x <= 0 || d->DisplaySize.y <= 0) {
        return;
    }
    const DkShader* vs = builtin_shader(kImguiVs);
    const DkShader* fs = builtin_shader(kImguiFs);
    if (!vs || !fs) return;
    // all lists' vertices, then all indices, into the stream ring
    const uint32_t vBytes = uint32_t(d->TotalVtxCount) * sizeof(ImDrawVert);
    const uint32_t iBytes = uint32_t(d->TotalIdxCount) * sizeof(ImDrawIdx);
    StreamAlloc vtx = stream_alloc(vBytes, 16), idx = stream_alloc(iBytes, 16),
                ubo = stream_alloc(256, DK_UNIFORM_BUF_ALIGNMENT);
    if (!vtx || !idx || !ubo) return;
    size_t vOff = 0, iOff = 0;
    for (int n = 0; n < d->CmdListsCount; n++) {
        const ImDrawList* l = d->CmdLists[n];
        memcpy(static_cast<uint8_t*>(vtx.cpu) + vOff, l->VtxBuffer.Data, size_t(l->VtxBuffer.Size) * sizeof(ImDrawVert));
        memcpy(static_cast<uint8_t*>(idx.cpu) + iOff, l->IdxBuffer.Data, size_t(l->IdxBuffer.Size) * sizeof(ImDrawIdx));
        vOff += size_t(l->VtxBuffer.Size) * sizeof(ImDrawVert);
        iOff += size_t(l->IdxBuffer.Size) * sizeof(ImDrawIdx);
    }
    // ImGui's pixels to normalized coordinates with y down; imgui_vsh negates y (clip-space y is up, dk.h)
    const float xform[4] = {2.0f / d->DisplaySize.x, 2.0f / d->DisplaySize.y,
                            -1.0f - d->DisplayPos.x * 2.0f / d->DisplaySize.x,
                            -1.0f - d->DisplayPos.y * 2.0f / d->DisplaySize.y};
    memcpy(ubo.cpu, xform, sizeof xform);
    const DkShader* sh[] = {vs, fs};
    dkCmdBufBindShaders(R.cmd, DkStageFlag_GraphicsMask, sh, 2);
    bind_2d_state(true);
    const DkViewport vp = {0, 0, float(R.width), float(R.height), 0.0f, 1.0f};
    dkCmdBufSetViewports(R.cmd, 0, &vp, 1);
    dkCmdBufBindUniformBuffer(R.cmd, DkStage_Vertex, 0, ubo.gpu, 256);
    static const DkVtxAttribState attribs[] = {
        DkVtxAttribState{0, 0, offsetof(ImDrawVert, pos), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
        DkVtxAttribState{0, 0, offsetof(ImDrawVert, uv), DkVtxAttribSize_2x32, DkVtxAttribType_Float, 0},
        DkVtxAttribState{0, 0, offsetof(ImDrawVert, col), DkVtxAttribSize_4x8, DkVtxAttribType_Unorm, 0},
    };
    static const DkVtxBufferState buffers[] = {DkVtxBufferState{sizeof(ImDrawVert), 0}};
    dkCmdBufBindVtxAttribState(R.cmd, attribs, 3);
    dkCmdBufBindVtxBufferState(R.cmd, buffers, 1);
    dkCmdBufBindVtxBuffer(R.cmd, 0, vtx.gpu, vBytes);
    dkCmdBufBindIdxBuffer(R.cmd, sizeof(ImDrawIdx) == 2 ? DkIdxFormat_Uint16 : DkIdxFormat_Uint32, idx.gpu);
    // ImGui's display (Aurora: the window, 1280x720) onto the swapchain image; scissors in window pixels
    const float sx = float(R.width) / d->DisplaySize.x, sy = float(R.height) / d->DisplaySize.y;
    uint32_t bound = 0;
    int vtxBase = 0, idxBase = 0;
    for (int n = 0; n < d->CmdListsCount; n++) {
        const ImDrawList* l = d->CmdLists[n];
        for (const ImDrawCmd& c : l->CmdBuffer) {
            if (c.UserCallback) continue;  // ImGui's reset-render-state callback: nothing to reset here
            float x0 = (c.ClipRect.x - d->DisplayPos.x) * sx, y0 = (c.ClipRect.y - d->DisplayPos.y) * sy;
            float x1 = (c.ClipRect.z - d->DisplayPos.x) * sx, y1 = (c.ClipRect.w - d->DisplayPos.y) * sy;
            x0 = std::max(x0, 0.0f);
            y0 = std::max(y0, 0.0f);
            x1 = std::min(x1, float(R.width));
            y1 = std::min(y1, float(R.height));
            if (x1 <= x0 || y1 <= y0) continue;
            const uint64_t id = uint64_t(c.GetTexID());
            if (id < kOverlayFirstImage || id >= kOverlayFirstImage + kOverlayImages || !g_tex[id].used) continue;
            if (id != bound) {
                const DkResHandle h = dkMakeTextureHandle(uint32_t(id), kSamplerLinearClamp);
                dkCmdBufBindTexture(R.cmd, DkStage_Fragment, 0, h);
                bound = uint32_t(id);
            }
            const DkScissor sc = {uint32_t(x0), uint32_t(y0), uint32_t(x1 - x0), uint32_t(y1 - y0)};
            dkCmdBufSetScissors(R.cmd, 0, &sc, 1);
            dkCmdBufDrawIndexed(R.cmd, DkPrimitive_Triangles, c.ElemCount, 1, c.IdxOffset + uint32_t(idxBase),
                                int32_t(c.VtxOffset) + vtxBase, 0);
        }
        vtxBase += l->VtxBuffer.Size;
        idxBase += l->IdxBuffer.Size;
    }
}

}  // namespace swdk

extern "C" uint64_t aurora_switch_dk_imgui_texture(uint32_t width, uint32_t height, const void* rgba8) {
    using namespace swdk;
    if (width == 0 || height == 0 || rgba8 == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g_pendingMutex);
    if (g_nextSlot >= kOverlayFirstImage + kOverlayImages) {
        dklog("ImGui: no free texture slot for a %ux%u texture (%u in use)", width, height, kOverlayImages);
        return 0;
    }
    const uint32_t slot = g_nextSlot++;
    const uint8_t* p = static_cast<const uint8_t*>(rgba8);
    g_pending.push_back({slot, width, height, std::vector<uint8_t>(p, p + size_t(width) * height * 4)});
    return slot;
}
