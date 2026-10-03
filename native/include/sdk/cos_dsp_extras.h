// The SDK-internal DSP task names Aurora's <dolphin/dsp.h> lacks.
//
// Phase 3, step 3.7b (JAudio; docs/NATIVE_PORT_PHASE2_3.md, decision D4). The name dolphin/dsp.h
// is both the game's and Aurora's, so Aurora's wins and no forwarder can sit in front of it. JAudio's
// osdsp.c and osdsp_task.c replace the SDK's DSPAddTask and __DSPHandler and use the SDK's task
// list directly, which the game's dolphin/dsp.h declares. cos_sdk defines all of these with C linkage
// (native/sdk/src/audio/DSPStubs.cpp, step 2.6f). A JAudio unit that uses them includes this header
// after dolphin/dsp.h under `TARGET_PC` (the GameCube build keeps the game's own dsp.h).
#ifndef COS_DSP_EXTRAS_H
#define COS_DSP_EXTRAS_H

#include <dolphin/dsp.h>

#ifdef __cplusplus
extern "C" {
#endif

// Same declarations as the game's dolphin/dsp.h.
extern DSPTaskInfo* __DSP_tmp_task;
extern DSPTaskInfo* __DSP_last_task;
extern DSPTaskInfo* __DSP_first_task;
extern DSPTaskInfo* __DSP_curr_task;

void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next);
void __DSP_boot_task(DSPTaskInfo* task);
void __DSP_insert_task(DSPTaskInfo* task);
void __DSP_add_task(DSPTaskInfo* task);
void __DSP_remove_task(DSPTaskInfo* task);

#ifdef __cplusplus
}
#endif

#endif // COS_DSP_EXTRAS_H
