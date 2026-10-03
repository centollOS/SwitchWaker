// The SDK-internal DSP task names Aurora's <dolphin/dsp.h> lacks.
//
// Phase 3, step 3.7b (JAudio; docs/NATIVE_PORT_PHASE2_3.md, decision D4). The name dolphin/dsp.h
// is both the game's and Aurora's, so Aurora's wins and no forwarder can sit in front of it. JAudio's
// osdsp.c and osdsp_task.c replace the SDK's DSPAddTask and __DSPHandler and use the SDK's task
// list directly, which the game's dolphin/dsp.h declares. cos_sdk defines all of these with C linkage
// (native/sdk/src/audio/DSP.cpp, steps 2.6f and 5.A). A JAudio unit that uses them includes this
// header after dolphin/dsp.h under `TARGET_PC` (the GameCube build keeps the game's own dsp.h).
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

// The DSP control register (DSPCR, the console's __DSPRegs[5] at 0xCC00500A), which a host
// program cannot address: read it, and write it (bit 0x80, DSPINT, is written as 1 to acknowledge
// the DSP's interrupt). Step 5.A; not part of the SDK.
u16 COSDSPReadControlRegister(void);
void COSDSPWriteControlRegister(u16 value);

#ifdef __cplusplus
}
#endif

#endif // COS_DSP_EXTRAS_H
