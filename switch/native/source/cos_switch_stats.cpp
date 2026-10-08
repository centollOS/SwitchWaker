// cos_switch_gfx_stats (cos_switch.h): the Switch's graphics and disc counters in one struct for the
// harness's perf-switch and hitch lines. Each source keeps running totals; this only gathers them.
#include "cos_switch.h"

#include <cstddef>

#include <aurora/switch_stats.h>

// switch/dawn/patches/dawn-switch-gl-fence-queue.patch (Dawn's QueueGL.cpp).
extern "C" void dawn_switch_gl_queue_stats(uint64_t out[6]);
// switch/dawn/patches/dawn-switch-gl-command-stats.patch (Dawn's CommandBufferGL.cpp): the
// running totals in the order of its switch_stats::Counter.
extern "C" void dawn_switch_gl_cmd_stats(uint64_t* out, size_t count);
// switch/native/nod/nod_gcn.cpp.
extern "C" void cos_switch_nod_stats(uint64_t out[3]);
#if defined(COS_SWITCH_DEKO3D)
// The deko3d NRO: its encoder's and GPU timestamps' figures go into the same fields the perf lines read
// (switch/deko/dk_aurora.h), in place of Dawn GL's, which stay zero there.
#include "dk_aurora.h"
#endif

extern "C" void cos_switch_gfx_stats(CosSwitchGfxStats* out) {
    AuroraSwitchStats a{};
    aurora_switch_get_stats(&a);
    uint64_t gl[6] = {};
    dawn_switch_gl_queue_stats(gl);
    uint64_t cmd[76] = {};
    dawn_switch_gl_cmd_stats(cmd, 76);
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
        .glFences = gl[0],
        .glWaits = gl[1],
        .glWaitNs = gl[2],
        .glFinishes = gl[3],
        .glFinishNs = gl[4],
        .glFencesPending = gl[5],
        .dvdReads = dvd[0],
        .dvdBytes = dvd[1],
        .dvdNs = dvd[2],
        .glPasses = cmd[0],
        .glDraws = cmd[1],
        .glPipelines = cmd[2],
        .glBindGroups = cmd[3],
        .glTexBinds = cmd[4],
        .glTexParams = cmd[5],
        .glTexParamsSkipped = cmd[6],
        .glUniforms = cmd[7],
        .glBufCopies = cmd[8],
        .glBufCopyBytes = cmd[9],
        .glTexUploads = cmd[10],
        .glExecuteNs = cmd[11],
        .glFlushNs = cmd[12],
        .glFlushItems = cmd[13],
        .glReleaseNs = cmd[14],
        .glPipelineNs = cmd[15],
        .glBindGroupNs = cmd[16],
        .glImmediatesNs = cmd[17],
        .glVertexStateNs = cmd[18],
        .glDrawCallNs = cmd[19],
        .glDrawsAfterPipeline = cmd[20],
        .glDrawAfterPipelineNs = cmd[21],
        .glDrawsAfterTextures = cmd[22],
        .glDrawAfterTexturesNs = cmd[23],
        .glUniformBufferBinds = cmd[24],
        .glVertexArrayBinds = cmd[25],
        .glIndexBufferBinds = cmd[26],
        .glPassLazyClearNs = cmd[27],
        .glPassFramebufferNs = cmd[28],
        .glPassDefaultStateNs = cmd[29],
        .glPassClearNs = cmd[30],
        .glPassEndNs = cmd[31],
        .glPassDynamicStateNs = cmd[32],
        .glPassTotalNs = cmd[33],
        .glBufCopyNs = cmd[34],
        .glBufCopiesBeforeFirstPass = cmd[35],
        .glBufCopyBeforeFirstPassNs = cmd[36],
        .glFirstBufCopyNs = cmd[37],
        .glTexCopies = cmd[38],
        .glTexCopyNs = cmd[39],
        .glFirstPasses = cmd[40],
        .glFirstPassNs = cmd[41],
        .glFirstPassLazyClearNs = cmd[42],
        .glFirstPassFramebufferNs = cmd[43],
        .glFirstPassDefaultStateNs = cmd[44],
        .glFirstPassClearNs = cmd[45],
        .glFirstPassEndNs = cmd[46],
        .glFirstPassReplayNs = cmd[47],
        .gpuFrames = cmd[48],
        .gpuTotalNs = cmd[49],
        .gpuEfbNs = cmd[50],
        .gpuTexConvNs = cmd[51],
        .gpuPresentNs = cmd[52],
        .gpuImguiNs = cmd[53],
        .gpuCopyNs = cmd[54],
        .gpuOtherNs = cmd[55],
        .gpuFirstPassNs = cmd[56],
        .gpuDisjoint = cmd[57],
        .gpuDropped = cmd[58],
        .gpuTimerState = cmd[59],
        .glDeferDeletes = cmd[60],
        .glDeferDeletesDone = cmd[61],
        .glDeferDeletesForced = cmd[62],
        .glDeferDeletePending = cmd[63],
        .glDeferDeleteState = cmd[64],
        .glPresents = cmd[68],
        .glPresentBlitNs = cmd[69],
        .glPresentSwapNs = cmd[70],
        .glTexUploadNs = cmd[71],
        .glCompressedUploadCalls = cmd[72],
        .glCompressedRepacks = cmd[73],
        .glTexCreates = cmd[74],
        .glTexCreateNs = cmd[75],
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
#if defined(COS_SWITCH_DEKO3D)
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
#endif
}
