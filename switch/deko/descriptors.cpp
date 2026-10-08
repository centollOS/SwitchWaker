// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/descriptors.cpp (the slot free lists retired by frame; its Latte sampler
// translation is not used: phase 3 builds samplers from Aurora's TextureBind).
// Image and sampler descriptor slots (dk.h). The sets live in memory.cpp's descriptor block and are
// bound at every frame's start. Slots below kReservedImages / kReservedSamplers belong to the
// renderer's own passes; the rest are handed out here, and a freed slot is reused once the GPU has
// finished the frame that freed it. Every write is a command (dkCmdBufPushData): the CPU never
// writes a descriptor the GPU may be reading. Render worker only.
#include "dk.h"

#include <deque>
#include <vector>

namespace swdk {
namespace {

struct Retired {
    uint64_t frame;  // the frame being recorded when the slot was freed
    uint32_t slot;
};
std::vector<uint32_t> g_freeImages;
uint32_t g_nextImage = kReservedImages;
std::deque<Retired> g_retired;
DescriptorStats g_stats;
bool g_samplersWritten = false;

}  // namespace

void descriptors_frame_start() {
    dkCmdBufBindImageDescriptorSet(R.cmd, image_descriptors(), kImageDescriptors);
    dkCmdBufBindSamplerDescriptorSet(R.cmd, sampler_descriptors(), kSamplerDescriptors);
    if (!g_samplersWritten) {
        // the renderer's samplers: 0 linear, 1 nearest, both clamped to the edge
        DkSamplerDescriptor d[2];
        for (int i = 0; i < 2; i++) {
            DkSampler s;
            dkSamplerDefaults(&s);
            s.minFilter = s.magFilter = i == kSamplerNearestClamp ? DkFilter_Nearest : DkFilter_Linear;
            s.wrapMode[0] = s.wrapMode[1] = s.wrapMode[2] = DkWrapMode_ClampToEdge;
            dkSamplerDescriptorInitialize(&d[i], &s);
        }
        dkCmdBufPushData(R.cmd, sampler_descriptors(), d, sizeof d);
        dkCmdBufBarrier(R.cmd, DkBarrier_None, DkInvalidateFlags_Descriptors);
        g_stats.samplerWrites += 2;
        g_samplersWritten = true;
    }
    // slots whose last frame the GPU has finished go back to the free list
    while (!g_retired.empty() && g_retired.front().frame <= R.frame && frame_done(g_retired.front().frame)) {
        g_freeImages.push_back(g_retired.front().slot);
        g_retired.pop_front();
    }
}

void write_image_descriptor(uint32_t slot, const DkImage& image) {
    DkImageView view;
    dkImageViewDefaults(&view, &image);
    DkImageDescriptor d;
    dkImageDescriptorInitialize(&d, &view, false, false);
    dkCmdBufPushData(R.cmd, image_descriptors() + slot * sizeof(DkImageDescriptor), &d, sizeof d);
    dkCmdBufBarrier(R.cmd, DkBarrier_None, DkInvalidateFlags_Descriptors);
    g_stats.imageWrites++;
}

uint32_t image_slot_alloc() {
    uint32_t slot = 0;
    if (!g_freeImages.empty()) {
        slot = g_freeImages.back();
        g_freeImages.pop_back();
    } else if (g_nextImage < kImageDescriptors) {
        slot = g_nextImage++;
    } else {
        static int logged = 0;
        if (logged++ < 10) dklog("descriptors: all %u image slots in use", kImageDescriptors);
        return 0;
    }
    g_stats.imagesUsed++;
    return slot;
}

void image_slot_free(uint32_t slot) {
    if (slot < kReservedImages || slot >= kImageDescriptors) return;
    g_retired.push_back({R.frame + 1, slot});
    g_stats.imagesUsed--;
}

DescriptorStats descriptor_stats() { return g_stats; }

}  // namespace swdk
