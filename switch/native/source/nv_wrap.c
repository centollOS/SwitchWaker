// Where the render worker waits inside Mesa (docs/SWITCH_PERF_STUDY.md, section 7): the NRO links
// with -Wl,--wrap for the few calls through which Mesa 20.1 and the Switch's libdrm_nouveau reach
// the GPU or block on it, and counts them per thread kind (Aurora's render worker / every other
// thread). No behaviour change: each wrapper times the real call and returns its result.
//
// - nouveau_pushbuf_space (Mesa -> libdrm): called when a command needs more push-buffer room than
//   the current 512 KiB chunk has left (PUSH_SPACE), and explicitly by a few kicks. Switching to
//   the next chunk of the 4-chunk ring kicks the full one and maps the next, which waits
//   (nouveau_bo_map -> nouveau_bo_wait -> nvFenceWait) until the GPU has retired the last
//   submission that used it. Its time therefore includes those waits.
// - nvFenceWait (libdrm -> libnx): every blocking wait of libdrm_nouveau on the GPU (push-buffer
//   chunk reuse, bo maps, bo deletes); timeout 0 calls are polls.
// - nvGpuChannelKickoff (libdrm -> libnx): one submission to the GPU (an nvdrv ioctl, so an IPC
//   round trip to nvservices).
// - nvGpuChannelAppendEntry (libdrm -> libnx): the GPFIFO entries of a submission; their command
//   words are the push-buffer bytes the GPU reads (Mesa's commands plus libdrm's fence/flush lists).
#include <stdatomic.h>
#include <stdint.h>
#include <switch.h>

#include "cos_switch.h"

struct nouveau_pushbuf;

// The bucket of the calling thread: 0 Aurora's render worker, 1 every other thread.
int cos_switch_thread_is_render(void);

struct NvBucket {
    atomic_ullong spaceCalls, spaceNs;
    atomic_ullong fenceWaits, fenceWaitNs, fencePolls;
    atomic_ullong kicks, kickNs;
    atomic_ullong pushWords;
};
static struct NvBucket g_nv[2];

static inline struct NvBucket* bucket(void) { return &g_nv[cos_switch_thread_is_render() ? 0 : 1]; }

static inline uint64_t ticks_ns(uint64_t start) { return armTicksToNs(armGetSystemTick() - start); }

int __real_nouveau_pushbuf_space(struct nouveau_pushbuf* push, uint32_t dwords, uint32_t relocs,
                                 uint32_t pushes);
int __wrap_nouveau_pushbuf_space(struct nouveau_pushbuf* push, uint32_t dwords, uint32_t relocs,
                                 uint32_t pushes) {
    const uint64_t start = armGetSystemTick();
    const int result = __real_nouveau_pushbuf_space(push, dwords, relocs, pushes);
    struct NvBucket* b = bucket();
    atomic_fetch_add_explicit(&b->spaceCalls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&b->spaceNs, ticks_ns(start), memory_order_relaxed);
    return result;
}

Result __real_nvFenceWait(NvFence* f, s32 timeout_us);
Result __wrap_nvFenceWait(NvFence* f, s32 timeout_us) {
    struct NvBucket* b = bucket();
    if (timeout_us == 0) {
        atomic_fetch_add_explicit(&b->fencePolls, 1, memory_order_relaxed);
        return __real_nvFenceWait(f, timeout_us);
    }
    const uint64_t start = armGetSystemTick();
    const Result result = __real_nvFenceWait(f, timeout_us);
    atomic_fetch_add_explicit(&b->fenceWaits, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&b->fenceWaitNs, ticks_ns(start), memory_order_relaxed);
    return result;
}

Result __real_nvGpuChannelKickoff(NvGpuChannel* c);
Result __wrap_nvGpuChannelKickoff(NvGpuChannel* c) {
    const uint64_t start = armGetSystemTick();
    const Result result = __real_nvGpuChannelKickoff(c);
    struct NvBucket* b = bucket();
    atomic_fetch_add_explicit(&b->kicks, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&b->kickNs, ticks_ns(start), memory_order_relaxed);
    return result;
}

Result __real_nvGpuChannelAppendEntry(NvGpuChannel* c, iova_t start, size_t num_cmds, u32 flags,
                                      u32 flush_threshold);
Result __wrap_nvGpuChannelAppendEntry(NvGpuChannel* c, iova_t start, size_t num_cmds, u32 flags,
                                      u32 flush_threshold) {
    atomic_fetch_add_explicit(&bucket()->pushWords, num_cmds, memory_order_relaxed);
    return __real_nvGpuChannelAppendEntry(c, start, num_cmds, flags, flush_threshold);
}

void cos_switch_nv_stats(uint64_t out[COS_SWITCH_NV_STATS]) {
    for (int i = 0; i < 2; i++) {
        const struct NvBucket* b = &g_nv[i];
        uint64_t* o = out + i * 8;
        o[0] = atomic_load_explicit(&b->spaceCalls, memory_order_relaxed);
        o[1] = atomic_load_explicit(&b->spaceNs, memory_order_relaxed);
        o[2] = atomic_load_explicit(&b->fenceWaits, memory_order_relaxed);
        o[3] = atomic_load_explicit(&b->fenceWaitNs, memory_order_relaxed);
        o[4] = atomic_load_explicit(&b->fencePolls, memory_order_relaxed);
        o[5] = atomic_load_explicit(&b->kicks, memory_order_relaxed);
        o[6] = atomic_load_explicit(&b->kickNs, memory_order_relaxed);
        o[7] = atomic_load_explicit(&b->pushWords, memory_order_relaxed);
    }
}
