// cos_switch_gfx_stats (cos_switch.h): the Switch's graphics and disc counters in one struct for the
// harness's perf-switch and hitch lines. Each source keeps running totals; this only gathers them.
#include "cos_switch.h"

#include <cstddef>

#include <aurora/switch_stats.h>

// switch/native/nod/nod_gcn.cpp.
extern "C" void cos_switch_nod_stats(uint64_t out[3]);
// The deko3d encoder's and GPU timestamps' figures (switch/deko/dk_aurora.h) go into the fields the
// perf lines read, named after the removed GL NRO's Dawn counters; those only Dawn GL had stay zero.
#include "dk_aurora.h"

extern "C" void cos_switch_gfx_stats(CosSwitchGfxStats* out) {
    AuroraSwitchStats a{};
    aurora_switch_get_stats(&a);
    uint64_t dvd[3] = {};
    cos_switch_nod_stats(dvd);
    uint64_t cpu[COS_SWITCH_THREAD_ROLES] = {};
    cos_switch_thread_cpu_ns(cpu);
    uint64_t nv[COS_SWITCH_NV_STATS] = {};
    cos_switch_nv_stats(nv);
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
        .dvdReads = dvd[0],
        .dvdBytes = dvd[1],
        .dvdNs = dvd[2],
        .cpuGameNs = cpu[COS_SWITCH_THREAD_GAME],
        .cpuRenderNs = cpu[COS_SWITCH_THREAD_RENDER],
        .cpuAudioNs = cpu[COS_SWITCH_THREAD_AUDIO],
        .cpuDvdNs = cpu[COS_SWITCH_THREAD_DVD],
        .cpuOtherNs = cpu[COS_SWITCH_THREAD_OTHER],
        .cpuCompileNs = cpu[COS_SWITCH_THREAD_COMPILE],
        .nvSpaceCalls = {nv[0], nv[8]},
        .nvSpaceNs = {nv[1], nv[9]},
        .nvFenceWaits = {nv[2], nv[10]},
        .nvFenceWaitNs = {nv[3], nv[11]},
        .nvFencePolls = {nv[4], nv[12]},
        .nvKicks = {nv[5], nv[13]},
        .nvKickNs = {nv[6], nv[14]},
        .nvPushWords = {nv[7], nv[15]},
    };
    uint64_t dk[AURORA_SWITCH_DK_STAT_COUNT] = {};
    aurora_switch_dk_gfx_stats(dk, AURORA_SWITCH_DK_STAT_COUNT);
    out->glPasses = dk[AURORA_SWITCH_DK_PASSES];
    out->glDraws = dk[AURORA_SWITCH_DK_DRAWS];
    out->glPipelines = dk[AURORA_SWITCH_DK_PIPELINE_BINDS];
    out->glBindGroups = dk[AURORA_SWITCH_DK_UNIFORM_BINDS];
    out->glTexBinds = dk[AURORA_SWITCH_DK_TEXTURE_BINDS];
    out->glUniforms = dk[AURORA_SWITCH_DK_UNIFORM_BINDS];
    out->glTexUploads = dk[AURORA_SWITCH_DK_UPLOADS];
    out->glTexCopies = dk[AURORA_SWITCH_DK_COPIES] + dk[AURORA_SWITCH_DK_CONVERSIONS];
    out->glExecuteNs = dk[AURORA_SWITCH_DK_ENCODE_NS];
    out->glFlushItems = dk[AURORA_SWITCH_DK_SUBMITS];
    out->gpuFrames = dk[AURORA_SWITCH_DK_GPU_FRAMES];
    out->gpuTotalNs = dk[AURORA_SWITCH_DK_GPU_TOTAL_NS];
    out->gpuEfbNs = dk[AURORA_SWITCH_DK_GPU_EFB_NS];
    out->gpuTexConvNs = dk[AURORA_SWITCH_DK_GPU_CONVERSION_NS];
    out->gpuPresentNs = dk[AURORA_SWITCH_DK_GPU_PRESENT_NS];
    out->gpuImguiNs = dk[AURORA_SWITCH_DK_GPU_IMGUI_NS];
    out->gpuCopyNs = dk[AURORA_SWITCH_DK_GPU_COPY_NS];
    out->gpuOtherNs = dk[AURORA_SWITCH_DK_GPU_OTHER_NS];
    out->gpuFirstPassNs = 0;
    out->gpuDisjoint = 0;
    out->gpuDropped = dk[AURORA_SWITCH_DK_GPU_DROPPED];
    // 1: timestamps read back (COS_DK_GPU_TIMERS=0 leaves gpuFrames at 0)
    out->gpuTimerState = 1;
}
