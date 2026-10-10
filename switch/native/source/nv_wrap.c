// Where a thread waits on the GPU through libnx (docs/SWITCH_PERF_STUDY.md, section 7, written for
// the removed GL NRO's Mesa and libdrm_nouveau): the NRO links with -Wl,--wrap for the few libnx calls
// that reach the GPU or block on it, and counts them per thread kind (Aurora's render worker / every
// other thread). No behaviour change: each wrapper times the real call and returns its result.
//
// - nvFenceWait: every blocking wait on a GPU fence; timeout 0 calls are polls.
// - nvGpuChannelKickoff: one submission to the GPU (an nvdrv ioctl, so an IPC round trip to
//   nvservices).
// - nvGpuChannelAppendEntry: the GPFIFO entries of a submission; their command words are the
//   push-buffer bytes the GPU reads.
// (The push-buffer space counters, from Mesa's nouveau_pushbuf_space, stay zero.)
#include <stdatomic.h>
#include <stdint.h>
#include <switch.h>

#include "cos_switch.h"

// The bucket of the calling thread: 0 Aurora's render worker, 1 every other thread.
int cos_switch_thread_is_render(void);

struct NvBucket {
    atomic_ullong fenceWaits, fenceWaitNs, fencePolls;
    atomic_ullong kicks, kickNs;
    atomic_ullong pushWords;
};
static struct NvBucket g_nv[2];

static inline struct NvBucket* bucket(void) { return &g_nv[cos_switch_thread_is_render() ? 0 : 1]; }

static inline uint64_t ticks_ns(uint64_t start) { return armTicksToNs(armGetSystemTick() - start); }

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
        o[0] = 0;  // push-buffer space calls and ns (Mesa's, gone with the GL NRO)
        o[1] = 0;
        o[2] = atomic_load_explicit(&b->fenceWaits, memory_order_relaxed);
        o[3] = atomic_load_explicit(&b->fenceWaitNs, memory_order_relaxed);
        o[4] = atomic_load_explicit(&b->fencePolls, memory_order_relaxed);
        o[5] = atomic_load_explicit(&b->kicks, memory_order_relaxed);
        o[6] = atomic_load_explicit(&b->kickNs, memory_order_relaxed);
        o[7] = atomic_load_explicit(&b->pushWords, memory_order_relaxed);
    }
}
