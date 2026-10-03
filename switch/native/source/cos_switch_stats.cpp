// cos_switch_gfx_stats (cos_switch.h): the Switch's graphics and disc counters in one struct for the
// harness's perf-switch and hitch lines. Each source keeps running totals; this only gathers them.
#include "cos_switch.h"

#include <aurora/switch_stats.h>

// switch/dawn/patches/dawn-switch-gl-fence-queue.patch (Dawn's QueueGL.cpp).
extern "C" void dawn_switch_gl_queue_stats(uint64_t out[6]);
// switch/native/nod/nod_gcn.cpp.
extern "C" void cos_switch_nod_stats(uint64_t out[3]);

extern "C" void cos_switch_gfx_stats(CosSwitchGfxStats* out) {
    AuroraSwitchStats a{};
    aurora_switch_get_stats(&a);
    uint64_t gl[6] = {};
    dawn_switch_gl_queue_stats(gl);
    uint64_t dvd[3] = {};
    cos_switch_nod_stats(dvd);
    *out = CosSwitchGfxStats{
        .frameSlotWaitNs = a.frameSlotWaitNs,
        .stagingWaitNs = a.stagingWaitNs,
        .queueFullWaitNs = a.queueFullWaitNs,
        .workerBusyNs = a.workerBusyNs,
        .workerEncodeNs = a.workerEncodeNs,
        .workerEndFrameNs = a.workerEndFrameNs,
        .workerUnmapNs = a.workerUnmapNs,
        .workerAcquireNs = a.workerAcquireNs,
        .workerSubmitNs = a.workerSubmitNs,
        .workerPresentNs = a.workerPresentNs,
        .workerEventsNs = a.workerEventsNs,
        .workerFrames = a.workerFrames,
        .pipelineCompiles = a.pipelineCompiles,
        .pipelineCompileNs = a.pipelineCompileNs,
        .pipelineCompileMaxNs = a.pipelineCompileMaxNs,
        .glFences = gl[0],
        .glWaits = gl[1],
        .glWaitNs = gl[2],
        .glFinishes = gl[3],
        .glFinishNs = gl[4],
        .glFencesPending = gl[5],
        .dvdReads = dvd[0],
        .dvdBytes = dvd[1],
        .dvdNs = dvd[2],
    };
}
