// cos_sdk: the DSP library over an emulated DSP (step 5.A of docs/NATIVE_PORT_PHASE4_6.md,
// decisions H6 and H10; it replaces the silent DSPStubs.cpp of step 2.6f). Aurora has none.
//
// The DSP is Dolphin's high-level emulation, DSPHLE (native/dsp_hle, cos_dsp_hle.h): its boot
// ROM takes the SDK's task boot mails, recognises the uploaded ucode by its hash and runs
// Dolphin's version of it (for the game the JAudio ucode, CRC 0x86840740). JAudio's own DSP code
// (osdsp.c, osdsp_task.c, dsptask.c, dspproc.c) runs unchanged on top, as it does on the console:
// The game's JAudio1 cannot run without its DSP (H10).
//
// What the SDK reads and writes as DSP registers goes to the emulated DSP:
// - Mailboxes. DSPSendMailToDSP hands the mail to the ucode, which handles it before the call
//   returns (Dolphin's HLE is synchronous), so DSPCheckMailToDSP is 0 again at once and the
//   SDK's `while (DSPCheckMailToDSP() != 0);` loops end. DSPCheckMailFromDSP/DSPReadMailFromDSP
//   read the DSP's mail queue (high half first; reading the low half pops the mail).
// - The control register (DSPCR, __DSPRegs[5]). The DSP keeps reset, halt and init; cos_sdk
//   keeps the DSP interrupt bit (DSPINT, 0x80, written as 1 to acknowledge). The PI->DSP
//   interrupt (0x02) and the AI/ARAM interrupt bits (0x08, 0x20) are not modelled: nothing on
//   the host raises them. JAudio's __DSPHandler acknowledges through
//   COSDSPReadControlRegister/COSDSPWriteControlRegister (cos_dsp_extras.h), since __DSPRegs is
//   a console address.
// - Interrupts. When the DSP raises its interrupt, a host thread (the "DSP interrupt" thread)
//   calls the handler DSPInit installed for __OS_INTERRUPT_DSP_DSP (JAudio's __DSPHandler) with
//   interrupts disabled (the OS lock), as the console's interrupt dispatcher does, and again while
//   the bit stays set. Like the alarm thread, it is "in interrupt context": it never blocks in
//   the OS, and OSEnableInterrupts inside the handler is ignored. The same thread lets the ucode
//   do its periodic work (cos_dsp_hle::Update) every millisecond.
// - Main memory as the DSP sees it is MEM1 by physical address (an offset into Aurora's MEM1
//   block, as OSPhysicalToCached), ARAM is Aurora's ARAM buffer (ARGetStorageAddress), so the
//   DSP is created at DSPInit, once both exist.
// - Tasks: the SDK's own task list and boot/exec mail sequences (decomp src/dolphin/dsp/dsp.c,
//   dsp_task.c). DSPAddTask is weak, as Aurora declares it: the game's JAudio defines its own
//   (osdsp.c), which wins when JAudio is linked.
//
// Locking: the emulated DSP is guarded by its own mutex (DspMutex()), always taken last, so a thread
// may use the mailboxes with interrupts disabled (DSPSendCommands2 does) while the interrupt
// thread waits for the OS lock. SDK task-list state is guarded by the OS lock, as the SDK
// disables interrupts.
//
// Provenance: the task list and mail sequences follow the decomp's src/dolphin/dsp/dsp.c and
// dsp_task.c (DSPReset, DSPHalt, DSPUnhalt and DSPGetDMAStatus, which the decomp lacks, follow
// the same SDK's register writes); Dusklight (ref/dusklight, CC0) only defines
// __DSP_first_task and __DSP_curr_task (src/dusk/globals.cpp:31-34).
#include "../os/os_internal.h"

#include <dolphin/ar.h>
#include <dolphin/dsp.h>
#include <dolphin/os.h>

#include <aurora/aurora.h>

#include "cos_dsp_hle.h"
#include "cos_sdk/host_alloc.h"

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <cstdlib>
#include <thread>

namespace aurora {
// Aurora's configuration (lib/aurora.cpp); mem1Size is the size of the MEM1 block.
extern AuroraConfig g_config;
} // namespace aurora

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

u16 COSDSPReadControlRegister(void);
void COSDSPWriteControlRegister(u16 value);

// The DSP interrupt handler DSPInit installs. The SDK defines it in dsp_task.c (the task
// switcher); the game's JAudio replaces it with its own (osdsp_task.c), which wins over the weak one
// below. Without JAudio (cos_sdk_smoke) the weak one only acknowledges the interrupt.
void __DSPHandler(__OSInterrupt interrupt, OSContext* context);

} // extern "C"

namespace {

// DSPCR bits.
constexpr u16 kCrPiInt = 0x0002;   // CPU -> DSP interrupt (DSPAssertInt)
constexpr u16 kCrHalt = 0x0004;
constexpr u16 kCrAiInt = 0x0008;
constexpr u16 kCrAramInt = 0x0020;
constexpr u16 kCrDspInt = 0x0080;
constexpr u16 kCrDmaState = 0x0200;
constexpr u16 kCrInterruptBits = kCrPiInt | kCrAiInt | kCrAramInt | kCrDspInt;

// The locks live in host memory and are never destroyed: the detached interrupt thread (and the
// game's audio thread) may still use the DSP while the process runs its static destructors at
// exit, as OSThread.cpp's Lock() and OSAlarm.cpp's state do.
struct DspSync {
    std::mutex dsp;       // the emulated DSP; always the last lock taken
    std::mutex wakeMutex; // wakes the interrupt thread
    std::condition_variable wakeCv;
};

DspSync& Sync() {
    static DspSync* const sSync = cos_sdk::HostNew<DspSync>(); // never destroyed
    return *sSync;
}

// The emulated DSP's lock and the interrupt bit.
std::mutex& DspMutex() {
    return Sync().dsp;
}
std::atomic<bool> sDspInt{false};

// Wakes the interrupt thread.
bool sWake = false; // guarded by Sync().wakeMutex

// SDK state, guarded by Lock().
bool sInit = false;
bool sThreadStarted = false;
OSThread sIntThreadRecord;
HostThread sIntThreadHost;

// ---- the emulated DSP's view of the host ------------------------------------------------------

std::uint8_t* GuestPointer(std::uint32_t address, std::uint32_t size) {
    const u64 mem1Size = aurora::g_config.mem1Size;
    if (OSBaseAddress != 0 && u64(address) + size <= mem1Size) {
        return static_cast<std::uint8_t*>(OSPhysicalToCached(address));
    }
    COS_SDK_LOG_ONCE("DSP: main-memory range 0x%08x+0x%x is outside MEM1 (0x%llx bytes); the DSP "
                     "reads zeros and its writes are dropped (logged once)",
                     address, size, static_cast<unsigned long long>(mem1Size));
    return nullptr;
}

std::uint64_t Timebase() {
    return static_cast<std::uint64_t>(OSGetTime());
}

// Called inside a cos_dsp_hle call (DspMutex() held): flags the interrupt, wakes the thread.
void RaiseInterrupt() {
    sDspInt.store(true);
    {
        std::lock_guard<std::mutex> wake(Sync().wakeMutex);
        sWake = true;
    }
    Sync().wakeCv.notify_one();
}

// Creates the DSP (in its reset state) if it does not exist yet. Needs DspMutex().
void EnsureDspLocked() {
    if (cos_dsp_hle::IsInitialized()) {
        return;
    }
    if (OSBaseAddress == 0) {
        Fatal("DSP: MEM1 is not set up (OSInit has not run); the DSP reads main memory");
    }
    cos_dsp_hle::Host host;
    host.guestPointer = GuestPointer;
    host.aram = static_cast<std::uint8_t*>(ARGetStorageAddress());
    host.aramSize = ARGetSize();
    host.timebase = Timebase;
    host.interrupt = RaiseInterrupt;
    if (host.aram == nullptr) {
        Fatal("DSP: ARAM is not set up (ARInit has not run); the DSP reads ARAM");
    }
    if (!cos_dsp_hle::Initialize(host)) {
        Fatal("DSP: the emulated DSP could not be created (ARAM %p, 0x%x bytes; the size must be "
              "a power of two)",
              static_cast<void*>(host.aram), host.aramSize);
    }
    Log("DSP: Dolphin's DSPHLE created (ARAM 0x%x bytes, MEM1 0x%x bytes)", host.aramSize,
        aurora::g_config.mem1Size);
}

u16 ReadControlLocked() {
    EnsureDspLocked();
    u16 value = cos_dsp_hle::ReadControl() & u16(~kCrInterruptBits);
    if (sDspInt.load()) {
        value |= kCrDspInt;
    }
    return value;
}

void WriteControlLocked(u16 value) {
    EnsureDspLocked();
    if ((value & kCrDspInt) != 0) {
        sDspInt.store(false); // write 1 to acknowledge
    }
    cos_dsp_hle::WriteControl(value & u16(~kCrInterruptBits));
}

// ---- the DSP interrupt thread -----------------------------------------------------------------

unsigned IntGapUs() {
    static const unsigned gap = [] {
        const char* v = std::getenv("COS_DSP_INT_GAP_US");
        return v != nullptr ? (unsigned)std::strtoul(v, nullptr, 10) : 0u;
    }();
    return gap;
}

void* InterruptThreadMain(void*) {
    {
        std::lock_guard<std::mutex> os(Lock());
        tCurrent = &sIntThreadRecord;
        tHost = &sIntThreadHost;
    }
    for (;;) {
        {
            std::unique_lock<std::mutex> wake(Sync().wakeMutex);
            Sync().wakeCv.wait_for(wake, std::chrono::milliseconds(1), [] { return sWake; });
            sWake = false;
        }
        {
            std::lock_guard<std::mutex> dsp(DspMutex());
            cos_dsp_hle::Update();
        }
        // Every interrupt the DSP has pending is handled under one hold of the OS lock: the
        // console takes a pending interrupt as soon as the handler returns, before any thread
        // runs again. Reading a mail in the handler raises the interrupt for the next queued one
        // (Dolphin's CMailHandler::ReadDSPMailboxLow), so the loop drains the DSP's mail queue.
        // Releasing the lock between two handlers let the audio thread, woken by the first (the
        // frame-done mail, syncDSP), send its next DsyncFrame and the release-halt sync mails
        // before the second, DONE_RENDERING (0xDCD10005), was answered with 0xCDD10003: the ucode
        // still blocked its commands, saw a sync mail with no frame being rendered and halted
        // ("Sync mail received when rendering was not active"); no audio was rendered after that,
        // every sequence stopped, and the item-get message waited forever for its fanfare.
        if (sDspInt.load()) {
            std::unique_lock<std::mutex> os(Lock());
            tInterruptsDisabled = true; // the handler runs "in interrupt context"
            while (sDspInt.load()) {
                if (ShuttingDownLocked()) {
                    tInterruptsDisabled = false;
                    return nullptr;
                }
                const __OSInterruptHandler handler = __OSGetInterruptHandler(__OS_INTERRUPT_DSP_DSP);
                if (handler == nullptr) {
                    break;
                }
                OSContext context;
                std::memset(&context, 0, sizeof(context));
                handler(__OS_INTERRUPT_DSP_DSP, &context);
            }
            tInterruptsDisabled = false;
        }
        // COS_DSP_INT_GAP_US (a test knob, bug B33): a pause after each drain, with the OS lock
        // released, as long as a thread switch under load; with the lock released between two
        // handlers (before the fix) it made the DSP halt within seconds.
        if (const unsigned gap = IntGapUs()) {
            std::this_thread::sleep_for(std::chrono::microseconds(gap));
        }
    }
}

// Starts the interrupt thread. Needs Lock().
void StartInterruptThreadLocked() {
    if (sThreadStarted) {
        return;
    }
    sThreadStarted = true;
    sIntThreadHost.alarmThread = true; // interrupt context: must not block, Enable is ignored
    sIntThreadRecord.state = OS_THREAD_STATE_RUNNING;
    sIntThreadRecord.priority = sIntThreadRecord.base = OS_PRIORITY_MIN;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t handle;
    const int err = pthread_create(&handle, &attr, &InterruptThreadMain, nullptr);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        Fatal("DSPInit: pthread_create of the DSP interrupt thread failed (%d: %s)", err,
              std::strerror(err));
    }
}

void SendMailWait(u32 mail) {
    DSPSendMailToDSP(mail);
    while (DSPCheckMailToDSP() != 0) {
    }
}

} // namespace

extern "C" {

u32 DSPCheckMailToDSP(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    EnsureDspLocked();
    return cos_dsp_hle::ReadCpuMail() >> 31;
}

u32 DSPCheckMailFromDSP(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    EnsureDspLocked();
    return cos_dsp_hle::ReadDspMailHigh() >> 15;
}

u32 DSPReadCPUToDSPMbox(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    EnsureDspLocked();
    return cos_dsp_hle::ReadCpuMail();
}

u32 DSPReadMailFromDSP(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    EnsureDspLocked();
    const u32 high = cos_dsp_hle::ReadDspMailHigh();
    return (high << 16) | cos_dsp_hle::ReadDspMailLow();
}

void DSPSendMailToDSP(u32 mail) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    EnsureDspLocked();
    cos_dsp_hle::WriteCpuMail(mail);
}

void DSPAssertInt(void) {
    // The SDK sets DSPCR bit 1 (the CPU's interrupt to the DSP). Dolphin's HLE ucodes take every
    // mail when it is written and never wait for this interrupt, so it only clears the
    // write-1-to-acknowledge bits as the SDK's read-modify-write does.
    BOOL old = OSDisableInterrupts();
    std::lock_guard<std::mutex> dsp(DspMutex());
    WriteControlLocked(ReadControlLocked() & u16(~(kCrAiInt | kCrAramInt | kCrDspInt)));
    OSRestoreInterrupts(old);
}

__attribute__((weak)) void __DSPHandler(__OSInterrupt interrupt, OSContext* context) {
    (void)interrupt;
    (void)context;
    COS_SDK_LOG_ONCE("__DSPHandler: the SDK's task switcher is not implemented (the game's JAudio "
                     "defines its own); DSP interrupts are only acknowledged");
    COSDSPWriteControlRegister((COSDSPReadControlRegister() & u16(~0x28)) | kCrDspInt);
}

u16 COSDSPReadControlRegister(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    return ReadControlLocked();
}

void COSDSPWriteControlRegister(u16 value) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    WriteControlLocked(value);
}

void DSPInit(void) {
    __DSP_debug_printf("DSPInit(): Build Date: %s %s\n", "Sep  5 2002", "05:35:13");
    BOOL old = OSDisableInterrupts();
    if (sInit) {
        OSRestoreInterrupts(old);
        return;
    }
    __OSSetInterruptHandler(__OS_INTERRUPT_DSP_DSP, __DSPHandler);
    __OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_DSP);
    {
        std::lock_guard<std::mutex> dsp(DspMutex());
        u16 tmp = ReadControlLocked();
        tmp = (tmp & u16(~0xA8)) | 0x800;
        WriteControlLocked(tmp);
        tmp = ReadControlLocked();
        WriteControlLocked(tmp & u16(~0xAC)); // clears halt: the boot ROM's mail can be read
    }
    __DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task = nullptr;
    sInit = true;
    StartInterruptThreadLocked();
    OSRestoreInterrupts(old);
}

BOOL DSPCheckInit(void) {
    Guard guard;
    return sInit ? TRUE : FALSE;
}

void DSPReset(void) {
    BOOL old = OSDisableInterrupts();
    {
        std::lock_guard<std::mutex> dsp(DspMutex());
        WriteControlLocked((ReadControlLocked() & u16(~0xA8)) | 0x801); // reset, init
    }
    sInit = false;
    OSRestoreInterrupts(old);
}

void DSPHalt(void) {
    BOOL old = OSDisableInterrupts();
    {
        std::lock_guard<std::mutex> dsp(DspMutex());
        WriteControlLocked((ReadControlLocked() & u16(~0xA8)) | kCrHalt);
    }
    OSRestoreInterrupts(old);
}

void DSPUnhalt(void) {
    BOOL old = OSDisableInterrupts();
    {
        std::lock_guard<std::mutex> dsp(DspMutex());
        WriteControlLocked(ReadControlLocked() & u16(~0xAC));
    }
    OSRestoreInterrupts(old);
}

u32 DSPGetDMAStatus(void) {
    std::lock_guard<std::mutex> dsp(DspMutex());
    return ReadControlLocked() & kCrDmaState; // the HLE's DMAs end at once: always 0
}

__attribute__((weak)) DSPTaskInfo* DSPAddTask(DSPTaskInfo* task) {
    BOOL old = OSDisableInterrupts();
    __DSP_insert_task(task);
    task->state = 0;
    task->flags = 1;
    OSRestoreInterrupts(old);
    if (task == __DSP_first_task) {
        __DSP_boot_task(task);
    }
    return task;
}

DSPTaskInfo* DSPCancelTask(DSPTaskInfo* task) {
    Guard guard;
    task->flags |= 2; // the handler removes it at the task's next yield
    return task;
}

DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task) {
    // The SDK's version asks the running task to yield (DSPAssertInt) so the handler switches to
    // `task` (__DSP_rude_task). The game does not call it; JAudio switches tasks on its own.
    COS_SDK_LOG_ONCE("DSPAssertTask: not implemented (the game does not call it); task %p is not "
                     "switched to",
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
    if (curr != nullptr) {
        SendMailWait(u32(uintptr_t(curr->dram_mmem_addr)));
        SendMailWait(curr->dram_length);
        SendMailWait(curr->dram_addr);
    } else {
        SendMailWait(0);
        SendMailWait(0);
        SendMailWait(0);
    }
    SendMailWait(u32(uintptr_t(next->iram_mmem_addr)));
    SendMailWait(next->iram_length);
    SendMailWait(next->iram_addr);
    if (next->state == 0) {
        SendMailWait(next->dsp_init_vector);
        SendMailWait(0);
        SendMailWait(0);
        SendMailWait(0);
    } else {
        SendMailWait(next->dsp_resume_vector);
        SendMailWait(u32(uintptr_t(next->dram_mmem_addr)));
        SendMailWait(next->dram_length);
        SendMailWait(next->dram_addr);
    }
}

void __DSP_boot_task(DSPTaskInfo* task) {
    if (task == nullptr) {
        Fatal("__DSP_boot_task: NULL task"); // the SDK's ASSERT
    }
    while (DSPCheckMailFromDSP() == 0) {
    }
    const u32 mail = DSPReadMailFromDSP();
    if (mail != 0x8071FEED) {
        Fatal("__DSP_boot_task(): Failed to sync DSP on boot! (0x%08X)", mail); // the SDK's ASSERT
    }
    SendMailWait(0x80F3A001);
    SendMailWait(u32(uintptr_t(task->iram_mmem_addr)));
    SendMailWait(0x80F3C002);
    SendMailWait(task->iram_addr & 0xFFFF);
    SendMailWait(0x80F3A002);
    SendMailWait(task->iram_length);
    SendMailWait(0x80F3B002);
    SendMailWait(0);
    SendMailWait(0x80F3D001);
    SendMailWait(task->dsp_init_vector);
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
