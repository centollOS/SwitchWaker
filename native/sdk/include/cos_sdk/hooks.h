// cos_sdk: hooks between the SDK layer and the game glue (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a).
//
// Dusklight's OS glue reads game state directly (dusk::IsShuttingDown, JKRHeap, Tracy). cos_sdk
// never includes game headers, so those couplings are replaced by the functions below, which the
// game glue calls or sets (later phases).
#ifndef COS_SDK_HOOKS_H
#define COS_SDK_HOOKS_H

#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Shutdown ---------------------------------------------------------------------------------

// Marks the process as shutting down (Dusklight's dusk::IsShuttingDown = true). Afterwards:
// - OSSendMessage, OSJamMessage and OSReceiveMessage with OS_MESSAGE_BLOCK return FALSE instead of
//   waiting, and the threads already waiting in them wake up and return FALSE;
// - the alarm timer thread stops calling alarm handlers.
// Other waits (OSSleepThread, OSLockMutex, OSWaitCond, OSJoinThread) are not interrupted. Threads
// made by OSCreateThread are detached host threads, so none of them keeps the process alive.
// It cannot be undone.
void COSSdkRequestShutdown(void);
BOOL COSSdkIsShuttingDown(void);

// ---- Per-thread state -------------------------------------------------------------------------

// Called on every thread made by OSCreateThread, on that thread, right before its entry function.
// On the GameCube JKRThreadSwitch gives each thread its own current JKRHeap from the switch-thread
// callback. Host threads run in parallel and never "switch", so cos_sdk never calls the
// OSSetSwitchThreadCallback callback; the game glue uses this hook instead to set up per-thread
// state such as the thread's current heap. Returns the previous hook. NULL removes it.
typedef void (*COSSdkThreadStartHook)(OSThread* thread);
COSSdkThreadStartHook COSSdkSetThreadStartHook(COSSdkThreadStartHook hook);

// The callback last passed to OSSetSwitchThreadCallback (NULL if none), for the game glue.
OSSwitchThreadCallback COSSdkGetSwitchThreadCallback(void);

// ---- Reset (step 2.6b) ------------------------------------------------------------------------

// Called by OSResetSystem after the registered reset functions ran (final pass included), with
// interrupts back at the caller's level. On the GameCube OSResetSystem never returns: the console
// reboots (OS_RESET_RESTART), restarts the game (OS_RESET_HOTRESET) or stops (OS_RESET_SHUTDOWN).
// The game glue (phase 6) does the host equivalent here and must not return: it ends the process,
// or ends the calling OS thread (OSExitThread) while another thread restarts the game. If no hook
// is set, or the hook returns, OSResetSystem logs, calls COSSdkRequestShutdown and ends the process
// with exit code 0 (the game would otherwise spin forever after the call). Returns the previous
// hook. NULL removes it.
typedef void (*COSSdkResetHook)(int reset, u32 resetCode, BOOL forceMenu);
COSSdkResetHook COSSdkSetResetHook(COSSdkResetHook hook);

// Sets what OSGetResetCode returns (0 at process start: a cold boot), for game glue that restarts
// the game in a new process and passes the code along. OSResetSystem sets it itself: the reset code
// for OS_RESET_HOTRESET, OS_RESETCODE_RESTART (0x80000000) for OS_RESET_RESTART.
void COSSdkSetResetCode(u32 resetCode);

// ---- Error handlers (step 2.6b) ---------------------------------------------------------------

// The handler last installed with OSSetErrorHandler for `error` (NULL if none or out of range).
// No hardware exception reaches them on the host; game glue that maps host faults (signals) to
// GameCube errors calls them through this.
OSErrorHandler COSSdkGetErrorHandler(OSError error);

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_HOOKS_H
