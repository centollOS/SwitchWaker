// The deko3d renderer's entry points for Aurora (docs/DEKO3D_MIGRATION_PLAN.md, phase 2). The deko3d
// NRO (COS_SWITCH_RENDERER=deko3d) builds Aurora with AURORA_GFX_DEKO3D: Aurora keeps recording and
// "encoding" every frame against Dawn's Null device (no GPU work: phase 2's GX encoding is a no-op),
// and these functions own the GPU: the device, the swapchain on the default NWindow, the present
// pass (the test pattern, ImGui: the options menu, the loading screen, the FPS panel) and the DKSH
// cache. Aurora's Switch patch 0013 calls them; switch/deko implements them.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ImDrawData;

// Device, queue, memory, swapchain, the renderer's own shaders and the DKSH cache. From
// webgpu::initialize on the game thread, before the render worker presents. gxConfigVersion is
// Aurora's gx::GXPipelineConfigVersion (part of the DKSH cache's compiler id).
void aurora_switch_dk_init(uint32_t gxConfigVersion);

// One frame into the next swapchain image: clear, the test pattern (COS_DK_TEST_PATTERN=1), ImGui's
// draw data (may be null), submit and present. Render worker only, at the end of every frame.
void aurora_switch_dk_present(const struct ImDrawData* ui);

// An RGBA8 texture for ImGui (the font atlas, aurora_imgui_add_texture): the ImTextureID to draw it
// with. Any thread; the pixels are copied now and reach the GPU at the next present.
uint64_t aurora_switch_dk_imgui_texture(uint32_t width, uint32_t height, const void* rgba8);

// Waits for the GPU and releases the window (Aurora's shutdown).
void aurora_switch_dk_shutdown(void);

#ifdef __cplusplus
}
#endif
