// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/memory.cpp.
// deko3d memory (dk.h): the per-frame stream and command memory rings with a fence per frame, the
// image heap (64 MiB chunks, first-fit suballocation, frees deferred until the GPU is past the
// frame), blocks of their own (Aurora's staging, oversized uploads), the shader code block, the
// descriptor sets and the query block. The rings and fences are the render worker's; the image heap,
// the blocks and the code block are thread-safe (Aurora creates textures on its FIFO thread).
#include "dk.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

namespace swdk {
namespace {

// deko3d aborts the process when a creation fails and never returns null: the line before names
// what was being created
DkMemBlock make_block(uint32_t size, uint32_t flags, const char* what) {
    dklog("creating a memory block: %s, %u KiB (flags 0x%X)", what, size >> 10, flags);
    log_flush();
    DkMemBlockMaker m;
    dkMemBlockMakerDefaults(&m, R.device, size);
    m.flags = flags;
    DkMemBlock b = dkMemBlockCreate(&m);
    dklog("memory block: %s at GPU address 0x%llX", what, (unsigned long long)dkMemBlockGetGpuAddr(b));
    return b;
}

constexpr uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

// ---- per-frame slots: a fence, a stream slice and a command memory slice each
struct Slot {
    DkFence fence{};      // after the commands of the frame that used the slot last
    bool fenced = false;  // signalled once (an unused DkFence is not waited for)
    uint64_t frame = 0;   // that frame
    uint32_t streamUsed = 0;
    uint32_t cmdUsed = 0;  // command memory fed from the slice
    // blocks created when the slice ran out; fed again the next time the slot runs out
    std::vector<std::pair<DkMemBlock, uint32_t>> cmdOverflow;
    size_t cmdOverflowUsed = 0;
    std::vector<ImageAlloc> retired;   // image memory freed while the slot's frame was recorded
    std::vector<DkMemBlock> retiredBlocks;
};
Slot g_slots[kFrames];
uint32_t g_slot = 0;
bool g_inFrame = false;
DkMemBlock g_stream = nullptr, g_cmdMem = nullptr, g_code = nullptr, g_descriptors = nullptr, g_queries = nullptr;
uint8_t* g_streamCpu = nullptr;
DkGpuAddr g_streamGpu = 0;
uint64_t g_cmdFedThisFrame = 0;
MemoryStats g_stats;

// The fences are written by the queue at submit (render worker) and polled by frames_completed from
// any thread: both under this lock.
std::mutex g_fenceMutex;
std::atomic<uint64_t> g_completed{0};

std::mutex g_codeMutex;
uint32_t g_codeUsed = 0;

// frees from other threads, handed to the render worker's next frame
std::mutex g_deferredMutex;
std::vector<ImageAlloc> g_deferredImages;
std::vector<DkMemBlock> g_deferredBlocks;

// The command buffer asks for memory during a frame (frame_begin feeds the first 64 KB): the next
// 64 KB of this frame's slice, then the slot's overflow blocks.
void add_cmd_memory(void*, DkCmdBuf cmd, size_t minReqSize) {
    Slot& s = g_slots[g_slot];
    const uint32_t want = align_up(uint32_t(std::max<size_t>(minReqSize, kCmdChunk)), DK_MEMBLOCK_ALIGNMENT);
    if (s.cmdUsed + want <= kCmdSliceSize) {
        dkCmdBufAddMemory(cmd, g_cmdMem, g_slot * kCmdSliceSize + s.cmdUsed, want);
        s.cmdUsed += want;
        g_cmdFedThisFrame += want;
        return;
    }
    if (s.cmdOverflowUsed == 0) g_stats.cmdOverflows++;  // the frame's first overflow
    while (s.cmdOverflowUsed < s.cmdOverflow.size()) {
        auto [block, size] = s.cmdOverflow[s.cmdOverflowUsed++];
        if (size < want) continue;
        dkCmdBufAddMemory(cmd, block, 0, size);
        g_cmdFedThisFrame += size;
        return;
    }
    const uint32_t size = std::max<uint32_t>(want, 1u << 20);
    dklog("command memory: frame %llu needs more than its %u MiB slice; a %u KiB block for slot %u",
          (unsigned long long)R.frame + 1, kCmdSliceSize >> 20, size >> 10, g_slot);
    DkMemBlock b = make_block(size, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "command overflow");
    s.cmdOverflow.push_back({b, size});
    s.cmdOverflowUsed = s.cmdOverflow.size();
    dkCmdBufAddMemory(cmd, b, 0, size);
    g_cmdFedThisFrame += size;
}

// ---- image heap (g_heapMutex)
struct Chunk {
    DkMemBlock block;
    uint32_t size;
    std::vector<std::pair<uint32_t, uint32_t>> free;  // offset, size; sorted by offset
};
std::mutex g_heapMutex;
std::vector<Chunk> g_chunks;

void add_chunk(uint32_t size) {
    char what[64];
    snprintf(what, sizeof what, "image heap chunk %zu", g_chunks.size());
    g_chunks.push_back({make_block(size, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, what), size, {{0, size}}});
    g_stats.imageChunks = g_chunks.size();
}

void chunk_free(Chunk& c, uint32_t offset, uint32_t size) {
    auto it = std::lower_bound(c.free.begin(), c.free.end(), std::make_pair(offset, 0u));
    it = c.free.insert(it, {offset, size});
    if (it + 1 != c.free.end() && it->first + it->second == (it + 1)->first) {
        it->second += (it + 1)->second;
        c.free.erase(it + 1);
    }
    if (it != c.free.begin() && (it - 1)->first + (it - 1)->second == it->first) {
        (it - 1)->second += it->second;
        c.free.erase(it);
    }
}

// a polled fence, under g_fenceMutex
bool slot_done(const Slot& s) { return !s.fenced || dkFenceWait(const_cast<DkFence*>(&s.fence), 0) == DkResult_Success; }

void note_completed(uint64_t frame) {
    uint64_t c = g_completed.load(std::memory_order_relaxed);
    while (frame > c && !g_completed.compare_exchange_weak(c, frame, std::memory_order_acq_rel)) {
    }
}

}  // namespace

void memory_init() {
    DkCmdBufMaker cm;
    dkCmdBufMakerDefaults(&cm, R.device);
    cm.cbAddMem = add_cmd_memory;
    dklog("creating the command buffer");
    log_flush();
    R.cmd = dkCmdBufCreate(&cm);
    char what[96];
    snprintf(what, sizeof what, "command memory ring (%u frames)", kFrames);
    g_cmdMem = make_block(kFrames * kCmdSliceSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, what);
    snprintf(what, sizeof what, "stream ring (%u frames: the renderer's own per-frame data)", kFrames);
    g_stream = make_block(kFrames * kStreamSliceSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, what);
    g_streamCpu = static_cast<uint8_t*>(dkMemBlockGetCpuAddr(g_stream));
    g_streamGpu = dkMemBlockGetGpuAddr(g_stream);
    g_code = make_block(kCodeSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code,
                        "shader code");
    snprintf(what, sizeof what, "descriptors (%u images, %u samplers)", kImageDescriptors, kSamplerDescriptors);
    g_descriptors = make_block(align_up((kImageDescriptors + kSamplerDescriptors) * 32, DK_MEMBLOCK_ALIGNMENT),
                               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, what);
    g_queries = make_block(kQuerySize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "queries");
    std::lock_guard<std::mutex> lock(g_heapMutex);
    add_chunk(kImageChunkSize);  // the first image heap chunk now: its size shows in the set-up cost
}

void frame_begin(uint64_t frame) {
    g_slot = uint32_t(frame % kFrames);
    Slot& s = g_slots[g_slot];
    if (s.fenced) {
        for (int waited = 0;; waited++) {
            DkResult r;
            {
                std::lock_guard<std::mutex> lock(g_fenceMutex);
                r = dkFenceWait(&s.fence, 0);
            }
            if (r != DkResult_Success) {
                // waits outside the lock (frames_completed polls meanwhile): the fence is not rewritten
                // before this frame's own submit
                r = dkFenceWait(&s.fence, 2'000'000'000ll);
            }
            if (r == DkResult_Success) break;
            dklog("frame %llu: the GPU has not finished frame %llu after %d s (result %d)%s", (unsigned long long)frame,
                  (unsigned long long)(frame - kFrames), (waited + 1) * 2, int(r),
                  dkQueueIsInErrorState(R.queue) ? "; the queue is in an error state" : "");
            if (waited >= 4) {
                dklog("giving up on the fence: continuing (the slot's memory may still be in use)");
                break;
            }
        }
        note_completed(s.frame);
    }
    {
        std::lock_guard<std::mutex> lock(g_heapMutex);
        for (const ImageAlloc& a : s.retired)
            if (a.chunk >= 0) chunk_free(g_chunks[size_t(a.chunk)], a.offset, a.size);
    }
    s.retired.clear();
    for (DkMemBlock b : s.retiredBlocks) {
        g_stats.blockBytes -= dkMemBlockGetSize(b);
        dkMemBlockDestroy(b);
    }
    s.retiredBlocks.clear();
    // what other threads freed since the last frame: freed once this frame is done
    {
        std::lock_guard<std::mutex> lock(g_deferredMutex);
        s.retired.insert(s.retired.end(), g_deferredImages.begin(), g_deferredImages.end());
        s.retiredBlocks.insert(s.retiredBlocks.end(), g_deferredBlocks.begin(), g_deferredBlocks.end());
        g_deferredImages.clear();
        g_deferredBlocks.clear();
    }
    s.streamUsed = 0;
    s.frame = frame;
    // dkCmdBufClear rewinds to the start of the memory fed last (deko3d 0.5.0 keeps it), which belongs
    // to the previous frame: feed this slot's first chunk explicitly (as deko_examples' CCmdMemRing)
    dkCmdBufClear(R.cmd);
    dkCmdBufAddMemory(R.cmd, g_cmdMem, g_slot * kCmdSliceSize, kCmdChunk);
    s.cmdUsed = kCmdChunk;
    s.cmdOverflowUsed = 0;
    g_cmdFedThisFrame = kCmdChunk;
    g_inFrame = true;
}

void frame_end() {
    Slot& s = g_slots[g_slot];
    dkCmdBufSignalFence(R.cmd, &s.fence, true);
    s.fenced = true;
    g_inFrame = false;
    g_stats.frames++;
    g_stats.cmdBytesSum += g_cmdFedThisFrame;
    g_stats.cmdBytesMax = std::max(g_stats.cmdBytesMax, g_cmdFedThisFrame);
    g_stats.streamBytesMax = std::max<uint64_t>(g_stats.streamBytesMax, s.streamUsed);
}

// device.cpp's submit holds the same lock while the queue writes the fences
std::mutex& fence_mutex() { return g_fenceMutex; }

bool frame_done(uint64_t frame) {
    if (frame == 0 || frame <= g_completed.load(std::memory_order_acquire)) return true;
    Slot& s = g_slots[frame % kFrames];
    std::lock_guard<std::mutex> lock(g_fenceMutex);
    if (s.frame != frame) return s.frame > frame;  // reused: waited for in frame_begin
    if (s.fenced && slot_done(s)) {
        note_completed(frame);
        return true;
    }
    return false;
}

uint64_t frames_completed() {
    std::lock_guard<std::mutex> lock(g_fenceMutex);
    uint64_t done = g_completed.load(std::memory_order_acquire);
    for (const Slot& s : g_slots) {
        if (s.fenced && s.frame > done && slot_done(s)) note_completed(s.frame);
    }
    // a slot whose newest frame is done implies every earlier frame is (one queue, in order)
    return g_completed.load(std::memory_order_acquire);
}

StreamAlloc stream_alloc(uint32_t size, uint32_t alignment) {
    Slot& s = g_slots[g_slot];
    const uint32_t offset = align_up(s.streamUsed, std::max<uint32_t>(alignment, 4));
    if (!g_inFrame || offset + size > kStreamSliceSize) {
        static int logged = 0;
        if (logged++ < 20)
            dklog("stream: %u bytes do not fit in frame %llu's %u MiB slice (%u used)%s", size,
                  (unsigned long long)R.frame + 1, kStreamSliceSize >> 20, s.streamUsed,
                  g_inFrame ? "" : " (outside a frame)");
        g_stats.streamFull++;
        return {};
    }
    s.streamUsed = offset + size;
    const uint32_t at = g_slot * kStreamSliceSize + offset;
    return {g_streamCpu + at, g_streamGpu + at};
}

void image_tile_size_fix(DkImageLayoutMaker& m, uint32_t rows) {
    if (m.flags & (DkImageFlags_PitchLinear | DkImageFlags_CustomTileSize | DkImageFlags_UsageVideo)) return;
    if (m.type == DkImageType_1D || m.type == DkImageType_1DArray || m.type == DkImageType_3D ||
        m.type == DkImageType_Buffer)
        return;  // (3D images: deko3d's depth tile never shrinks at level 0)
    if (m.type == DkImageType_2DMS || m.type == DkImageType_2DMSArray) {
        if (m.msMode == DkMsMode_4x || m.msMode == DkMsMode_8x) rows *= 2;  // the multisampled height
    }
    // deko3d's choice (dkImageLayoutInitialize: pickTileSize of 1.5 x the height in GOBs) ...
    const uint32_t gobs = (rows + rows / 2 + 7) / 8;
    const uint32_t tile = gobs >= 16 ? 4 : gobs >= 8 ? 3 : gobs >= 4 ? 2 : gobs >= 2 ? 1 : 0;
    // ... and its level-0 shrink (calcLevelOffset: adjustTileSize(m_tileH, 8, height))
    uint32_t shrunk = tile;
    while (shrunk && (8u << (shrunk - 1)) >= rows) shrunk--;
    if (shrunk == tile) return;  // deko3d's own layout is consistent
    m.flags |= DkImageFlags_CustomTileSize;
    m.tileSize = DkTileSize(shrunk);
}

ImageAlloc image_alloc(uint32_t size, uint32_t alignment) {
    size = align_up(size, 256);
    alignment = std::max<uint32_t>(alignment, 256);
    std::lock_guard<std::mutex> lock(g_heapMutex);
    for (int pass = 0; pass < 2; pass++) {
        for (size_t ci = 0; ci < g_chunks.size(); ci++) {
            Chunk& c = g_chunks[ci];
            for (size_t i = 0; i < c.free.size(); i++) {
                auto [off, len] = c.free[i];
                const uint32_t at = align_up(off, alignment);
                if (at - off + size > len) continue;
                c.free.erase(c.free.begin() + long(i));
                if (at > off) chunk_free(c, off, at - off);
                if (at + size < off + len) chunk_free(c, at + size, off + len - at - size);
                g_stats.imageBytes += size;
                return {c.block, at, size, int(ci)};
            }
        }
        add_chunk(std::max(kImageChunkSize, align_up(size + alignment, DK_MEMBLOCK_ALIGNMENT)));
    }
    fatal("image heap: cannot place %u bytes", size);
}

void image_free_later(const ImageAlloc& a) {
    if (a.chunk < 0) return;
    {
        std::lock_guard<std::mutex> lock(g_heapMutex);
        g_stats.imageBytes -= a.size;
    }
    g_slots[g_slot].retired.push_back(a);
}

void image_free_deferred(const ImageAlloc& a) {
    if (a.chunk < 0) return;
    {
        std::lock_guard<std::mutex> lock(g_heapMutex);
        g_stats.imageBytes -= a.size;
    }
    std::lock_guard<std::mutex> lock(g_deferredMutex);
    g_deferredImages.push_back(a);
}

Block block_create(uint32_t size, const char* what) {
    size = align_up(std::max<uint32_t>(size, 1), DK_MEMBLOCK_ALIGNMENT);
    Block b;
    b.block = make_block(size, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, what);
    b.cpu = static_cast<uint8_t*>(dkMemBlockGetCpuAddr(b.block));
    b.gpu = dkMemBlockGetGpuAddr(b.block);
    b.size = size;
    std::lock_guard<std::mutex> lock(g_heapMutex);
    g_stats.blockBytes += size;
    return b;
}

void block_free_deferred(DkMemBlock b) {
    if (!b) return;
    std::lock_guard<std::mutex> lock(g_deferredMutex);
    g_deferredBlocks.push_back(b);
}

bool code_load(DkShader& shader, const void* dksh, uint32_t size, const char* name) {
    std::lock_guard<std::mutex> lock(g_codeMutex);
    const uint32_t at = align_up(g_codeUsed, DK_SHADER_CODE_ALIGNMENT);
    if (at + size > kCodeSize - DK_SHADER_CODE_UNUSABLE_SIZE) {
        static int logged = 0;
        if (logged++ < 10) dklog("shader code memory full: %s (%u bytes) not loaded", name, size);
        return false;
    }
    uint8_t* const dst = static_cast<uint8_t*>(dkMemBlockGetCpuAddr(g_code)) + at;
    memcpy(dst, dksh, size);
    DkShaderMaker m;
    dkShaderMakerDefaults(&m, g_code, at);
    dkShaderInitialize(&shader, &m);
    if (!dkShaderIsValid(&shader)) {
        dklog("shader %s (%u bytes of DKSH) is not valid", name, size);
        return false;
    }
    g_codeUsed = at + size;
    g_stats.codeBytes = g_codeUsed;
    return true;
}

uint32_t code_used() {
    std::lock_guard<std::mutex> lock(g_codeMutex);
    return g_codeUsed;
}

DkGpuAddr image_descriptors() { return dkMemBlockGetGpuAddr(g_descriptors); }
DkGpuAddr sampler_descriptors() { return dkMemBlockGetGpuAddr(g_descriptors) + kImageDescriptors * 32; }

QueryRegion query_region(uint64_t frame) {
    constexpr uint32_t kRegion = kQuerySize / kFrames / 16 * 16;
    const uint32_t at = uint32_t(frame % kFrames) * kRegion;
    return {static_cast<uint8_t*>(dkMemBlockGetCpuAddr(g_queries)) + at, dkMemBlockGetGpuAddr(g_queries) + at, kRegion};
}

MemoryStats memory_stats_take() {
    MemoryStats s;
    {
        std::lock_guard<std::mutex> lock(g_heapMutex);
        s = g_stats;
    }
    {
        std::lock_guard<std::mutex> lock(g_codeMutex);
        s.codeBytes = g_codeUsed;
    }
    g_stats.cmdBytesMax = g_stats.cmdBytesSum = g_stats.cmdOverflows = 0;
    g_stats.streamBytesMax = g_stats.streamFull = g_stats.frames = 0;
    return s;
}

}  // namespace swdk
