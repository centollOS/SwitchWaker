/* The run harness's Switch platform layer (switch/native/source): what native/src/pc does with
 * macOS facilities (signals, backtrace(), the executable's directory, _Exit) done with libnx.
 * Plain C and no libnx types, so the harness (compiled like a game unit, with clang) includes it
 * without libnx's headers. */
#ifndef COS_SWITCH_H
#define COS_SWITCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The native port's directory on the SD card, without the "sdmc:" device: sqlite (Aurora's
 * caches) treats a path that does not start with '/' as relative. */
#define COS_SWITCH_ROOT "/switch/centollos/native"
/* The disc image, shared with the translated port (scripts/switch/push.sh --game). */
#define COS_SWITCH_DEFAULT_DISC "/switch/centollos/GZLE01.iso"

/* Ends the process with `code`, from any thread: flushes the logs to the SD card and the USB
 * host, then exits the process (svcExitProcess: the threads the game started cannot be stopped,
 * so the app does not return to the Homebrew Menu in the same process). */
__attribute__((noreturn)) void cos_switch_exit(int code);

/* Writes every queued log byte to the SD card (and gives the USB host up to a second). */
void cos_switch_flush_logs(void);

/* Load address of the NRO: an address minus this is the offset addr2line takes with centollos.elf. */
uintptr_t cos_switch_image_base(void);

/* Kernel thread ID of the calling thread. */
uint64_t cos_switch_thread_id(void);

/* Reads one 8-byte word if the address is mapped readable (svcQueryMemory); 0 otherwise. */
int cos_switch_read_word(uintptr_t addr, uintptr_t* out);

/* The crash report (libnx's exception handler) calls this to add the harness's state line. */
void cos_switch_set_crash_state_writer(void (*writer)(int fd));

/* Running totals (since start) of the Switch's graphics and disc counters, for the harness's
 * "[cos] perf-switch" and "[cos] hitch" lines (native/src/pc/pc_frame.cpp), which diff two reads.
 * Times in ns. Sources: Aurora's Switch patch 0005 (aurora_switch_get_stats), the Dawn GL queue
 * and command statistics patches (switch/dawn/patches) and the disc reader (nod/). */
typedef struct {
    /* aurora_begin_frame: waiting for a free frame slot / a mapped staging buffer; any producer
     * waiting for room in the render worker's queue. */
    uint64_t frameSlotWaitNs, stagingWaitNs, queueFullWaitNs;
    /* Aurora's render worker: busy time, GX pass encoding, EndFrame items and their staging
     * Unmap, surface acquire, Queue::Submit and Surface::Present; Instance::ProcessEvents; frames. */
    uint64_t workerBusyNs, workerEncodeNs, workerEndFrameNs, workerUnmapNs, workerAcquireNs;
    uint64_t workerSubmitNs, workerPresentNs, workerEventsNs, workerFrames;
    /* Pipelines created, the time it took, the longest one. */
    uint64_t pipelineCompiles, pipelineCompileNs, pipelineCompileMaxNs;
    /* Dawn's GL queue: fences made, blocking waits and their time, glFinish calls (only with
     * COS_SWITCH_GL_FINISH=1) and their time, fences not yet seen signaled (a level, not a total). */
    uint64_t glFences, glWaits, glWaitNs, glFinishes, glFinishNs, glFencesPending;
    /* Disc image reads (nod_read): calls, bytes, time. */
    uint64_t dvdReads, dvdBytes, dvdNs;
    /* Dawn's GL replay of the submissions (switch/dawn/patches/dawn-switch-gl-command-stats.patch):
     * render passes, draws, pipeline applies (glUseProgram + fixed state), bind group applies,
     * sampled-texture binds, glTexParameteri issued and skipped as unchanged, glUniform uploads of
     * immediates, buffer-to-buffer copies and their bytes, buffer-to-texture copies; the time of
     * CommandBuffer::Execute, of the whole deferred-work flush (Execute and the other deferred GL
     * work such as buffer map/unmap, creations and writes, plus the context release) and of the
     * context release alone; deferred work items run. */
    uint64_t glPasses, glDraws, glPipelines, glBindGroups, glTexBinds, glTexParams, glTexParamsSkipped;
    uint64_t glUniforms, glBufCopies, glBufCopyBytes, glTexUploads;
    uint64_t glExecuteNs, glFlushNs, glFlushItems, glReleaseNs;
    /* Where the render passes' replay time goes (switch/dawn/patches/dawn-switch-gl-replay-timers.patch):
     * pipeline applies, bind group applies, immediates, vertex/index state, the glDraw* calls (Mesa
     * validates the state set before a draw inside the call); the draws that are the first after a
     * pipeline change and the other draws right after a texture bind, with their glDraw* time;
     * glBindBufferRange of uniform buffers, glBindVertexArray and index buffer binds issued. */
    uint64_t glPipelineNs, glBindGroupNs, glImmediatesNs, glVertexStateNs, glDrawCallNs;
    uint64_t glDrawsAfterPipeline, glDrawAfterPipelineNs, glDrawsAfterTextures, glDrawAfterTexturesNs;
    uint64_t glUniformBufferBinds, glVertexArrayBinds, glIndexBufferBinds;
    /* The parts of Execute the replay timers leave out (switch/dawn/patches/dawn-switch-gl-pass-timers.patch):
     * before a render pass, texture synchronisation and lazy clears; the framebuffer set-up (Gen/
     * Bind/attachments/DrawBuffers); the default dynamic state; the LoadOp::Clear clears; the pass
     * end (resolve, DeleteFramebuffers); SetViewport/SetScissorRect/SetBlendConstant/
     * SetStencilReference; whole passes (lazy clears included). Buffer-to-buffer copies: time, how
     * many ran before the Execute's first render pass and their time, the first copy of each
     * Execute; texture-to-texture copies and their time. */
    uint64_t glPassLazyClearNs, glPassFramebufferNs, glPassDefaultStateNs, glPassClearNs, glPassEndNs;
    uint64_t glPassDynamicStateNs, glPassTotalNs;
    uint64_t glBufCopyNs, glBufCopiesBeforeFirstPass, glBufCopyBeforeFirstPassNs, glFirstBufCopyNs;
    uint64_t glTexCopies, glTexCopyNs;
    /* The same split for the first render pass of each Execute alone (one per frame): count, whole
     * pass, lazy clears, framebuffer set-up, default state, clears, end, and its replay (pipelines,
     * bind groups, immediates, vertex state, draws, viewport/scissor/blend). */
    uint64_t glFirstPasses, glFirstPassNs, glFirstPassLazyClearNs, glFirstPassFramebufferNs;
    uint64_t glFirstPassDefaultStateNs, glFirstPassClearNs, glFirstPassEndNs, glFirstPassReplayNs;
    /* GPU time from GL_TIME_ELAPSED_EXT queries (switch/dawn/patches/dawn-switch-gl-gpu-timer.patch),
     * read back a few frames late: frames read, their GPU time in all and per kind of segment (the
     * EFB passes, the EFB copy conversions/blits, the present pass, the ImGui pass, copies between
     * passes, other passes) and of each frame's first render pass; frames dropped for a disjoint
     * event or not timed because results were not coming back. gpuTimerState is a level: 0 no
     * Execute yet, 1 on, 2 the driver has no GL_EXT_disjoint_timer_query, 3 COS_SWITCH_GPU_TIMER=0. */
    uint64_t gpuFrames, gpuTotalNs, gpuEfbNs, gpuTexConvNs, gpuPresentNs, gpuImguiNs, gpuCopyNs;
    uint64_t gpuOtherNs, gpuFirstPassNs, gpuDisjoint, gpuDropped, gpuTimerState;
    /* Deferred GL deletes (switch/dawn/patches/dawn-switch-gl-deferred-delete.patch): texture and
     * buffer names whose deletion waited for the GPU, deleted, deleted early (too many pending);
     * pending now and the state (levels: 0 not used yet, 1 on, 2 COS_SWITCH_GL_DEFER_DELETE=0). */
    uint64_t glDeferDeletes, glDeferDeletesDone, glDeferDeletesForced, glDeferDeletePending;
    uint64_t glDeferDeleteState;
    /* CPU time (ns, the kernel's per-thread tick count) of the game thread, Aurora's render worker,
     * JAudio's audio thread, the game's DVD thread and every other thread together. */
    uint64_t cpuGameNs, cpuRenderNs, cpuAudioNs, cpuDvdNs, cpuOtherNs, cpuCompileNs;
    /* Mesa's and libdrm_nouveau's waits and submissions (switch/native/source/nv_wrap.c), for the
     * render worker [0] and every other thread [1]: nouveau_pushbuf_space calls (a push-buffer chunk
     * filled; includes the wait for the GPU to free the next chunk of the ring) and their time,
     * blocking nvFenceWait calls and their time, polls, GPU submissions (nvGpuChannelKickoff) and
     * their time, command words submitted. */
    uint64_t nvSpaceCalls[2], nvSpaceNs[2], nvFenceWaits[2], nvFenceWaitNs[2], nvFencePolls[2];
    uint64_t nvKicks[2], nvKickNs[2], nvPushWords[2];
} CosSwitchGfxStats;

#define COS_SWITCH_NV_STATS 16
void cos_switch_nv_stats(uint64_t out[COS_SWITCH_NV_STATS]);

void cos_switch_gfx_stats(CosSwitchGfxStats* out);

/* Threads by role, for their CPU time (switch/native/source/thread_wrap.c). Every pthread is
 * registered as OTHER when it starts; a thread names its role with cos_switch_thread_role. */
enum {
    COS_SWITCH_THREAD_OTHER = 0, /* Aurora's DVD worker and pipeline threads, Dawn's, the logs... */
    COS_SWITCH_THREAD_GAME,      /* the game's main thread */
    COS_SWITCH_THREAD_RENDER,    /* Aurora's render worker (all of Dawn's GL work) */
    COS_SWITCH_THREAD_AUDIO,     /* JAudio's audio thread (JASystem::TAudioThread) */
    COS_SWITCH_THREAD_DVD,       /* the game's DVD thread (mDoDvdThd) */
    COS_SWITCH_THREAD_COMPILE,   /* Aurora's pipeline compile thread (Switch patch 0010) */
    COS_SWITCH_THREAD_ROLES
};
void cos_switch_thread_role(int role);

/* "handheld, gpu 307.2 MHz, emc 1331.2 MHz" (operation mode, GPU and memory controller clocks now)
 * into out; snprintf's result. Asks the clock service: call it once per perf window, not per frame. */
int cos_switch_describe_mode(char* out, size_t size);
/* CPU time (ns, svcGetInfo ThreadTickCount) of the registered threads per role, running totals;
 * a thread that ended keeps its last value. */
void cos_switch_thread_cpu_ns(uint64_t out[COS_SWITCH_THREAD_ROLES]);
/* "render#3 c2/0x4 27.6, other#5 c1/0x7 9.1 (fn 0x1234), ..." into out: every thread that used at
 * least 0.3 ms of CPU per frame since the last call (frames = game frames since then), with its
 * preferred core, affinity mask and, for unnamed threads, its entry point (offset in centollos.elf). */
int cos_switch_thread_table(char* out, size_t size, double frames);

#ifdef __cplusplus
}
#endif

#endif /* COS_SWITCH_H */
