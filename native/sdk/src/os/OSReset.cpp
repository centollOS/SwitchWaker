// cos_sdk: reset and reboot (docs/NATIVE_PORT_PHASE2_3.md, step 2.6b): reset functions,
// OSResetSystem, the reset code, the reset button and the save region.
//
// Provenance: adapted from Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight). There
// OSResetSystem logs, sets dusk::IsShuttingDown, wakes the message queues and returns;
// OSGetResetCode, OSGetResetSwitchState and OSGetResetButtonState return 0, and OSSetSaveRegion
// does nothing. Changed:
// - OSRegisterResetFunction/OSUnregisterResetFunction keep the SDK's priority-ordered queue, and
//   OSResetSystem runs it as the SDK does (non-final passes until every function is ready, then
//   the final pass with interrupts disabled);
// - OSResetSystem never returns, as on the console (the game's mDoRst_reset spins in VIWaitForRetrace
//   after it): it hands over to the game glue's reset hook (hooks.h), and without one it ends the
//   process after COSSdkRequestShutdown, with a log line;
// - the reset code and the save region survive into the next run of the game when the hook
//   restarts it in the same process (OSGetResetCode, OSGetSavedRegion), as they survive a reset
//   on the console.
// No reset button exists on the host: OSGetResetSwitchState and OSGetResetButtonState return FALSE
// and the OSSetResetCallback callback is never called.
#include "os_internal.h"

#include "cos_sdk/hooks.h"
#include "cos_sdk/sram.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace cos_sdk::os;

namespace {

// All guarded by Lock().
OSResetFunctionQueue sResetQueue = {nullptr, nullptr};
COSSdkResetHook sResetHook = nullptr;
u32 sResetCode = 0;
OSResetCallback sResetCallback = nullptr;
void* sSaveStart = nullptr;
void* sSaveEnd = nullptr;
void* sSavedStart = nullptr;
void* sSavedEnd = nullptr;

// Runs every reset function in queue order. True when all of them returned TRUE. The functions
// run without the OS lock held for the non-final pass (they may wait for devices and threads),
// as on the console, where that pass runs with interrupts enabled.
bool CallResetFunctions(BOOL final) {
    OSResetFunctionInfo* info;
    {
        Guard guard;
        info = sResetQueue.head;
    }
    bool ok = true;
    while (info != nullptr) {
        if (!info->func(final)) {
            ok = false;
        }
        Guard guard;
        info = info->next;
    }
    return ok && __OSSyncSram(); // as the SDK: the SRAM must be written back too
}

} // namespace

extern "C" {

void OSRegisterResetFunction(OSResetFunctionInfo* info) {
    Guard guard;
    // As the SDK: ordered by priority (lower first); a new function goes after those of the same
    // priority.
    OSResetFunctionInfo* next = sResetQueue.head;
    while (next != nullptr && next->priority <= info->priority) {
        next = next->next;
    }
    info->next = next;
    if (next == nullptr) {
        info->prev = sResetQueue.tail;
        if (sResetQueue.tail != nullptr) {
            sResetQueue.tail->next = info;
        } else {
            sResetQueue.head = info;
        }
        sResetQueue.tail = info;
    } else {
        info->prev = next->prev;
        next->prev = info;
        if (info->prev != nullptr) {
            info->prev->next = info;
        } else {
            sResetQueue.head = info;
        }
    }
}

void OSUnregisterResetFunction(OSResetFunctionInfo* info) {
    Guard guard;
    if (info->next != nullptr) {
        info->next->prev = info->prev;
    } else {
        sResetQueue.tail = info->prev;
    }
    if (info->prev != nullptr) {
        info->prev->next = info->next;
    } else {
        sResetQueue.head = info->next;
    }
    info->next = nullptr;
    info->prev = nullptr;
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    if (reset != OS_RESET_RESTART && reset != OS_RESET_HOTRESET && reset != OS_RESET_SHUTDOWN) {
        Fatal("OSResetSystem: unknown reset type %d", reset);
    }
    Log("OSResetSystem(reset=%d, resetCode=0x%x, forceMenu=%d)", reset,
        static_cast<unsigned>(resetCode), static_cast<int>(forceMenu));

    // As the SDK: repeat the non-final pass until every reset function is ready.
    while (!CallResetFunctions(FALSE)) {
        std::this_thread::yield();
    }
    if (reset == OS_RESET_HOTRESET && forceMenu) {
        SramSetForceMenu();
    }

    const BOOL level = OSDisableInterrupts();
    CallResetFunctions(TRUE);
    LCDisable();
    if (reset == OS_RESET_HOTRESET) {
        sResetCode = resetCode;
    } else if (reset == OS_RESET_RESTART) {
        sResetCode = OS_RESETCODE_RESTART;
    }
    sSavedStart = sSaveStart;
    sSavedEnd = sSaveEnd;
    const COSSdkResetHook hook = sResetHook;
    OSRestoreInterrupts(level);

    // Nothing with a destructor is alive here: the hook may end this thread (OSExitThread).
    if (hook != nullptr) {
        hook(reset, resetCode, forceMenu);
        Log("OSResetSystem: the reset hook returned; ending the process");
    } else {
        Log("OSResetSystem: no reset hook is set (COSSdkSetResetHook), so the game cannot be "
            "restarted; ending the process");
    }
    COSSdkRequestShutdown();
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(0);
}

u32 OSGetResetCode(void) {
    Guard guard;
    return sResetCode;
}

BOOL OSGetResetSwitchState(void) {
    return FALSE;
}

BOOL OSGetResetButtonState(void) {
    return FALSE;
}

OSResetCallback OSSetResetCallback(OSResetCallback callback) {
    if (callback != nullptr) {
        COS_SDK_LOG_ONCE("OSSetResetCallback: there is no reset button on the host; the callback "
                         "is never called");
    }
    Guard guard;
    const OSResetCallback old = sResetCallback;
    sResetCallback = callback;
    return old;
}

void OSSetSaveRegion(void* start, void* end) {
    Guard guard;
    sSaveStart = start;
    sSaveEnd = end;
}

void OSGetSaveRegion(void** start, void** end) {
    Guard guard;
    *start = sSaveStart;
    *end = sSaveEnd;
}

void OSGetSavedRegion(void** start, void** end) {
    Guard guard;
    *start = sSavedStart;
    *end = sSavedEnd;
}

COSSdkResetHook COSSdkSetResetHook(COSSdkResetHook hook) {
    Guard guard;
    const COSSdkResetHook old = sResetHook;
    sResetHook = hook;
    return old;
}

void COSSdkSetResetCode(u32 resetCode) {
    Guard guard;
    sResetCode = resetCode;
}

} // extern "C"
