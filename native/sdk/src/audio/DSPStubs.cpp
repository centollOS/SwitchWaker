// cos_sdk: the DSP library, silent (docs/NATIVE_PORT_PHASE2_3.md, step 2.6f). Aurora has none.
// No DSP is emulated: JAudio's DSP microcode never runs. Phase 5 replaces JAudio's DSP side
// (osdsp.c, osdsp_task.c, dsptask.c, dspproc.c) with host mixing; until then these functions only
// have to keep the SDK's bookkeeping and never block.
// - Mailboxes. CPU to DSP: DSPSendMailToDSP stores the mail and the DSP "takes" it at once, so
//   DSPCheckMailToDSP is always 0 and the SDK's `while (DSPCheckMailToDSP() != 0);` loops end at
//   once; DSPReadCPUToDSPMbox reads back the last mail. DSP to CPU: no mail ever arrives,
//   DSPCheckMailFromDSP is always 0 and DSPReadMailFromDSP returns 0.
// - Interrupts. DSPAssertInt does nothing and the DSP never interrupts the CPU, so __DSPHandler
//   (which JAudio's osdsp_task.c defines, replacing the SDK's) is never installed or called, and
//   with it the task callbacks (init_cb, res_cb, done_cb, req_cb) never run.
// - Tasks. The task list (__DSP_first_task, __DSP_last_task, __DSP_curr_task, __DSP_tmp_task,
//   __DSP_insert_task, __DSP_add_task, __DSP_remove_task) is the SDK's own algorithm. DSPInit
//   resets it as the SDK does. __DSP_boot_task and __DSP_exec_task, which on the console send the
//   task's IRAM/DRAM addresses to the DSP through the mailbox, log once that the task is not run
//   and return: the SDK's __DSP_boot_task first waits for the DSP's 0x8071FEED boot mail, which
//   would never come.
// What this means for JAudio (phase 3.7 links it over these functions, phase 5 makes it play):
// its DSP handshakes wait for DSP answers in busy loops of its own (DspHandShake's init_cb never
// runs, so DSPSendCommands2 waits in Dsp_Running_Check; DsetupTable and DsetDolbyDelay wait for a
// reply callback). Those loops are JAudio code and phase 5 replaces them; the log lines below say
// where to look if one is reached before then.
// - DSPAddTask is weak, as Aurora declares it: the game's JAudio defines its own (osdsp.c), which wins
//   when JAudio is linked. This one is the SDK's (insert by priority; boot it if it is first).
// - DSPCancelTask sets the cancel flag as the SDK does; with no DSP the task is never removed.
//   DSPAssertTask only logs once and returns the task.
// All state is guarded by the OS lock (interrupts disabled), as the SDK disables interrupts.
//
// Provenance: Dusklight (ref/dusklight, CC0) only defines __DSP_first_task and __DSP_curr_task
// (src/dusk/globals.cpp:31-34); Dusklight's JAudio2 there needs nothing else. The rest
// follows the decomp's src/dolphin/dsp/dsp.c and dsp_task.c (task list algorithms copied; the
// register accesses replaced by the mailbox model above).
#include "../os/os_internal.h"

#include <dolphin/dsp.h>

using namespace cos_sdk::os;

extern "C" {

// The SDK's task list (dolphin/dsp.h of the decomp declares these).
DSPTaskInfo* __DSP_curr_task;
DSPTaskInfo* __DSP_first_task;
DSPTaskInfo* __DSP_last_task;
DSPTaskInfo* __DSP_tmp_task;

void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next);
void __DSP_boot_task(DSPTaskInfo* task);
void __DSP_insert_task(DSPTaskInfo* task);
void __DSP_add_task(DSPTaskInfo* task);
void __DSP_remove_task(DSPTaskInfo* task);
void __DSP_debug_printf(const char* fmt, ...);

} // extern "C"

namespace {

// All guarded by Lock().
bool sInit = false;
u32 sLastMailToDSP = 0;

} // namespace

extern "C" {

u32 DSPCheckMailToDSP(void) {
    return 0; // the DSP takes every mail at once
}

u32 DSPCheckMailFromDSP(void) {
    return 0; // the DSP never sends mail
}

u32 DSPReadCPUToDSPMbox(void) {
    Guard guard;
    return sLastMailToDSP;
}

u32 DSPReadMailFromDSP(void) {
    COS_SDK_LOG_ONCE("DSPReadMailFromDSP: no DSP is emulated (phase 5); reading 0");
    return 0;
}

void DSPSendMailToDSP(u32 mail) {
    Guard guard;
    sLastMailToDSP = mail;
}

void DSPAssertInt(void) {}

void DSPInit(void) {
    Guard guard;
    if (sInit) {
        return;
    }
    COS_SDK_LOG_ONCE("DSPInit: no DSP is emulated yet (phase 5): DSP tasks are kept but never "
                     "run, and the DSP never sends mail or interrupts");
    __DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task = nullptr;
    sInit = true;
}

BOOL DSPCheckInit(void) {
    Guard guard;
    return sInit ? TRUE : FALSE;
}

void DSPReset(void) {
    Guard guard;
    sInit = false; // the SDK's DSPReset also halts the DSP; there is none to halt
}

// No DSP runs, so there is nothing to halt or resume.
void DSPHalt(void) {}

void DSPUnhalt(void) {}

u32 DSPGetDMAStatus(void) {
    return 0; // no DSP DMA is ever in progress
}

__attribute__((weak)) DSPTaskInfo* DSPAddTask(DSPTaskInfo* task) {
    Guard guard;
    __DSP_insert_task(task);
    task->state = 0;
    task->flags = 1;
    if (task == __DSP_first_task) {
        __DSP_boot_task(task);
    }
    return task;
}

DSPTaskInfo* DSPCancelTask(DSPTaskInfo* task) {
    Guard guard;
    task->flags |= 2; // as the SDK; the DSP would remove it at its next yield
    return task;
}

DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task) {
    COS_SDK_LOG_ONCE("DSPAssertTask: no DSP is emulated (phase 5); task %p is not switched to",
                     static_cast<void*>(task));
    return task;
}

DSPTaskInfo* __DSPGetCurrentTask(void) {
    Guard guard;
    return __DSP_curr_task;
}

void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next) {
    if (next == nullptr) {
        Fatal("__DSP_exec_task: NULL next task"); // the SDK's ASSERT
    }
    (void)curr;
    COS_SDK_LOG_ONCE("__DSP_exec_task: no DSP is emulated (phase 5); task %p is not run",
                     static_cast<void*>(next));
}

void __DSP_boot_task(DSPTaskInfo* task) {
    if (task == nullptr) {
        Fatal("__DSP_boot_task: NULL task"); // the SDK's ASSERT
    }
    COS_SDK_LOG_ONCE("__DSP_boot_task: no DSP is emulated (phase 5); task %p is not booted and "
                     "its callbacks will never be called",
                     static_cast<void*>(task));
}

void __DSP_insert_task(DSPTaskInfo* task) {
    Guard guard;
    if (__DSP_first_task == nullptr) {
        __DSP_curr_task = task;
        __DSP_first_task = __DSP_last_task = task;
        task->next = task->prev = nullptr;
        return;
    }
    DSPTaskInfo* temp = __DSP_first_task;
    while (temp != nullptr) {
        if (task->priority < temp->priority) {
            task->prev = temp->prev;
            temp->prev = task;
            task->next = temp;
            if (task->prev == nullptr) {
                __DSP_first_task = task;
            } else {
                task->prev->next = task;
            }
            break;
        }
        temp = temp->next;
    }
    if (temp == nullptr) {
        __DSP_last_task->next = task;
        task->next = nullptr;
        task->prev = __DSP_last_task;
        __DSP_last_task = task;
    }
}

void __DSP_add_task(DSPTaskInfo* task) {
    if (task == nullptr) {
        Fatal("__DSP_add_task: NULL task"); // the SDK's ASSERT
    }
    Guard guard;
    if (__DSP_last_task == nullptr) {
        __DSP_curr_task = task;
        __DSP_last_task = task;
        __DSP_first_task = task;
        task->next = task->prev = nullptr;
    } else {
        __DSP_last_task->next = task;
        task->next = nullptr;
        task->prev = __DSP_last_task;
        __DSP_last_task = task;
    }
    task->state = 0;
}

void __DSP_remove_task(DSPTaskInfo* task) {
    if (task == nullptr) {
        Fatal("__DSP_remove_task: NULL task"); // the SDK's ASSERT
    }
    Guard guard;
    task->flags = 0;
    task->state = 3;
    if (__DSP_first_task == task) {
        if (task->next != nullptr) {
            __DSP_first_task = task->next;
            task->next->prev = nullptr;
        } else {
            __DSP_first_task = __DSP_last_task = __DSP_curr_task = nullptr;
        }
        return;
    }
    if (__DSP_last_task == task) {
        __DSP_last_task = task->prev;
        task->prev->next = nullptr;
        __DSP_curr_task = __DSP_first_task;
        return;
    }
    __DSP_curr_task = task->next;
    task->prev->next = task->next;
    task->next->prev = task->prev;
}

void __DSP_debug_printf(const char* fmt, ...) {
    (void)fmt; // empty in the SDK's release build, as here
}

} // extern "C"
