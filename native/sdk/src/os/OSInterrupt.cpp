// cos_sdk: OSDisableInterrupts/OSEnableInterrupts/OSRestoreInterrupts and the interrupt handler
// table (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a; the model is in os_internal.h).
//
// Provenance: replaces the interrupt section of Dusklight src/dusk/OSThread.cpp (CC0,
// ref/dusklight), where all three functions return FALSE and do nothing, and its
// __OSSetInterruptHandler/__OSUnmaskInterrupts stubs. Here "interrupts disabled" is the OS lock
// that guards all of cos_sdk's OS state, held per thread with the GameCube's boolean semantics
// (Disable returns whether they were enabled; Restore(FALSE) keeps them disabled), so code that
// disables interrupts to protect data shared with another thread or an alarm handler is protected.
//
// No hardware raises interrupts on the host: the handler table only records what is installed
// (the devices of later steps call their callbacks directly).
#include "os_internal.h"

using namespace cos_sdk::os;

namespace {

bool sLoggedAlarmEnable = false;
__OSInterruptHandler sHandlers[__OS_INTERRUPT_MAX] = {};
OSInterruptMask sMaskedInterrupts = 0; // __OSMaskInterrupts/__OSUnmaskInterrupts
OSInterruptMask sUserMask = 0;         // OSSetInterruptMask

// Re-enables interrupts for the calling thread. Returns FALSE (they were disabled).
BOOL EnableHeld() {
    if (tHost != nullptr && tHost->alarmThread) {
        // An alarm handler runs with interrupts disabled and must return that way: the timer
        // thread still uses the lock. The GameCube would allow nested interrupts here.
        if (!sLoggedAlarmEnable) {
            sLoggedAlarmEnable = true;
            Log("an alarm handler enabled interrupts; ignored (handlers keep them disabled)");
        }
        return FALSE;
    }
    tInterruptsDisabled = false;
    Lock().unlock();
    return FALSE;
}

} // namespace

extern "C" {

BOOL OSDisableInterrupts(void) {
    if (tInterruptsDisabled) {
        return FALSE;
    }
    Lock().lock();
    tInterruptsDisabled = true;
    if (tCurrent == nullptr) {
        AdoptCurrentLocked();
    }
    CheckpointLocked();
    return TRUE;
}

BOOL OSEnableInterrupts(void) {
    if (!tInterruptsDisabled) {
        return TRUE;
    }
    return EnableHeld();
}

BOOL OSRestoreInterrupts(BOOL level) {
    if (level) {
        return OSEnableInterrupts();
    }
    return OSDisableInterrupts();
}

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX) {
        Fatal("__OSSetInterruptHandler: interrupt %d out of range", interrupt);
    }
    Guard guard;
    __OSInterruptHandler previous = sHandlers[interrupt];
    sHandlers[interrupt] = handler;
    return previous;
}

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX) {
        Fatal("__OSGetInterruptHandler: interrupt %d out of range", interrupt);
    }
    Guard guard;
    return sHandlers[interrupt];
}

OSInterruptMask __OSMaskInterrupts(OSInterruptMask mask) {
    Guard guard;
    const OSInterruptMask previous = sMaskedInterrupts;
    sMaskedInterrupts |= mask;
    return previous;
}

OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask) {
    Guard guard;
    const OSInterruptMask previous = sMaskedInterrupts;
    sMaskedInterrupts &= ~mask;
    return previous;
}

OSInterruptMask OSGetInterruptMask(void) {
    Guard guard;
    return sUserMask;
}

OSInterruptMask OSSetInterruptMask(OSInterruptMask mask) {
    Guard guard;
    const OSInterruptMask previous = sUserMask;
    sUserMask = mask;
    return previous;
}

} // extern "C"
