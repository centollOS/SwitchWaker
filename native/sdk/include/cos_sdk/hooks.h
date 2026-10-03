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

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_HOOKS_H
