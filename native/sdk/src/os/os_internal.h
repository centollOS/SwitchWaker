// cos_sdk: state shared by the host implementation of the OS thread, interrupt, mutex, message
// and alarm functions (native/sdk/src/os/*.cpp). Internal to cos_sdk.
//
// The model (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a):
//
// - On the GameCube the whole OS is protected by disabling interrupts: one CPU, and nothing else
//   can run while interrupts are off. Here one process-wide host mutex, Lock(), plays that part.
//   OSDisableInterrupts() takes it and OSRestoreInterrupts()/OSEnableInterrupts() give it back.
//   "Interrupts disabled" is a per-thread boolean (tInterruptsDisabled), as MSR[EE] is part of each
//   thread's context on the GameCube, so nested Disable/Restore pairs behave as on the console.
// - Every SDK function that touches OS state holds Lock() (a Guard), so all OS bookkeeping
//   (OSThread fields, thread queues, mutexes, message queues, the alarm queue) is serialised.
// - A thread that blocks (OSSleepThread, OSLockMutex, OSWaitCond, OSReceiveMessage, OSJoinThread,
//   self-suspension) waits on its own condition variable, which releases Lock() while it sleeps,
//   even when the caller had disabled interrupts. That matches the console, where the thread switch
//   restores the next thread's interrupt state. Each OSThread has a HostThread with that variable.
// - Alarms fire on a real host timer thread (decision D6), which calls the handlers while holding
//   Lock(), as an interrupt handler runs with interrupts disabled.
// - Threads made by OSCreateThread are detached host threads (pthreads). They are never joined at
//   host level: OSJoinThread waits on the OSThread state, as the console does.
//
// Differences from the console that game code can notice (phase 6 work, see the plan):
// - Threads run in parallel. OSResumeThread of a higher-priority thread does not run it at once,
//   and priorities do not order anything.
// - OSSuspendThread and OSCancelThread on another thread that is running game code take effect at
//   that thread's next cos_sdk OS call (a "checkpoint"); both are logged.
#ifndef COS_SDK_OS_INTERNAL_H
#define COS_SDK_OS_INTERNAL_H

#include <dolphin/os.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace cos_sdk::os {

// Host-side state for one OSThread. Every field is guarded by Lock().
struct HostThread {
    std::condition_variable cv; // waited on with Lock()
    void* (*func)(void*) = nullptr;
    void* param = nullptr;
    bool created = false;     // made by OSCreateThread: a host thread runs (or will run) it
    bool started = false;     // the host thread has been launched
    bool cancelled = false;   // OSCancelThread on another thread: exit at the next checkpoint
    bool alarmThread = false; // the alarm timer thread (handlers must not block)
};

// The OS lock ("interrupts disabled"). Never destroyed, so detached threads may use it at exit.
std::mutex& Lock();

// Whether the calling thread holds Lock(), i.e. has interrupts disabled.
extern thread_local bool tInterruptsDisabled;

// The calling thread's OSThread and HostThread. Null until the thread first calls the SDK.
extern thread_local OSThread* tCurrent;
extern thread_local HostThread* tHost;

// Takes Lock() unless the calling thread already holds it, gives the calling thread an OSThread
// if it has none (AdoptCurrentLocked), and runs the checkpoint (CheckpointLocked).
class Guard {
public:
    Guard();
    ~Guard();
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

private:
    bool mOwns;
};

// Gives the calling thread an OSThread: the first thread gets the default (main) thread, others
// get a record of their own that lives as long as the host thread. Needs Lock().
void AdoptCurrentLocked();

// Exits the calling thread if it was cancelled, and blocks it while it is suspended. Needs Lock().
void CheckpointLocked();

// Waits on the calling thread's condition variable until `ready()` holds and the thread is not
// suspended. A cancelled thread exits here. Needs Lock(); releases it while waiting.
template <class Pred>
void WaitLocked(Pred ready);

// The HostThread of `thread`, or null. Needs Lock().
HostThread* FindHostLocked(OSThread* thread);

// Wakes the host thread behind `thread` so it re-checks its wait condition. Needs Lock().
void NotifyLocked(OSThread* thread);

// OSSleepThread/OSWakeupThread with Lock() held. With `wakeOnShutdown`, SleepLocked also returns
// (with the thread taken off `queue`) once COSSdkRequestShutdown has been called; it then returns
// false.
bool SleepLocked(OSThreadQueue* queue, bool wakeOnShutdown);
void WakeupLocked(OSThreadQueue* queue);

// Ends the calling host thread (pthread_exit) after releasing Lock(). Only for OSCreateThread
// threads; aborts for others.
[[noreturn]] void ExitCurrentLocked();

// Mutex bookkeeping used by OSExitThread and OSCancelThread (OSMutex.cpp). Needs Lock().
void UnlockAllMutexLocked(OSThread* thread);

// COSSdkRequestShutdown has been called. Needs Lock().
bool ShuttingDownLocked();
// Wakes every waiting thread and the alarm thread after shutdown was requested. Needs Lock().
void NotifyAllLocked();
void NotifyAlarmThreadLocked();

// Sets the SRAM flag a hot reset with forceMenu sets (OSSram.cpp, for OSResetSystem).
void SramSetForceMenu();

// Logging to stderr with a "[cos_sdk]" prefix. Fatal aborts.
void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Log() only the first time this line is reached (any thread).
#define COS_SDK_LOG_ONCE(...)                                                                      \
    do {                                                                                           \
        static std::atomic<bool> cos_sdk_logged_{false};                                           \
        if (!cos_sdk_logged_.exchange(true)) {                                                     \
            ::cos_sdk::os::Log(__VA_ARGS__);                                                       \
        }                                                                                          \
    } while (0)

// ---------------------------------------------------------------------------------------------
// Intrusive OSThreadQueue helpers over OSThread::link (the console's macros, as functions).

void QueueRemove(OSThreadQueue* queue, OSThread* thread);
// Inserts in priority order (lower value first; equal priorities keep FIFO order).
void QueueAddPrio(OSThreadQueue* queue, OSThread* thread);

// ---------------------------------------------------------------------------------------------

template <class Pred>
void WaitLocked(Pred ready) {
    HostThread* host = tHost;
    OSThread* self = tCurrent;
    if (host->alarmThread) {
        Fatal("a blocking OS call was made from an alarm handler (handlers run with interrupts "
              "disabled and must not block)");
    }
    std::unique_lock<std::mutex> lock(Lock(), std::adopt_lock);
    host->cv.wait(lock, [&] { return host->cancelled || (ready() && self->suspend <= 0); });
    lock.release(); // Lock() stays held, as it was on entry
    if (host->cancelled) {
        ExitCurrentLocked();
    }
}

} // namespace cos_sdk::os

#endif // COS_SDK_OS_INTERNAL_H
