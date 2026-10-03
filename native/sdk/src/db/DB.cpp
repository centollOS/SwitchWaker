// cos_sdk: the DB (debugger) library and the debugger-link functions (docs/NATIVE_PORT_PHASE2_3.md,
// step 2.6e). Aurora declares them in <dolphin/db.h> and <dolphin/db/DBInterface.h> but defines
// none.
//
// On the console DBInit points __DBInterface at the debugger block the IPL or a debugger leaves at
// physical address 0x40 (bPresent, the exception mask, the exception entry). The host has no
// hardware debugger, so:
// - DBInit points __DBInterface at a host block with bPresent = 0 and no exception marked;
//   DBIsDebuggerPresent is FALSE. __DBSetPresent and __DBMarkException edit that block (they are
//   how a debugger stub would register itself), and __DBIsExceptionMarked reads it as the SDK
//   does. Before DBInit, __DBInterface is NULL as on the console, and the queries say "no".
// - DBPrintf does nothing, as in the SDK's release library (the decomp's db.c).
// - __DBExceptionDestination(Aux) is where the console sends an exception a debugger marked: the
//   SDK dumps the context and halts (PPCHalt). Host exceptions never get there; if code calls it,
//   it reports and aborts, the host's halt.
// - The debugger link (OdemuExi2's DBInitComm, DBInitInterrupts, DBQueryData, DBRead, DBWrite,
//   DBOpen, DBClose; only TRK uses it, and TRK is not in the native build): DBQueryData reports
//   no pending data (0), DBOpen/DBClose do nothing (as in OdemuExi2). DBInitComm and
//   DBInitInterrupts log once that there is no debugger link. DBRead and DBWrite log once and
//   return 1, a failure for TRK (which treats 0 as success): data written to a debugger that does
//   not exist must not look delivered. Aurora declares DBInitComm, DBQueryData and DBRead with
//   its own signatures; those are the ones defined. DBInitInterrupts, DBWrite, DBOpen and DBClose
//   are the game additions (native/include/sdk/dolphin/db/db.h), repeated here with the same signatures
//   because cos_sdk compiles against Aurora's headers only (sdk.cmake).
//
// Provenance: not in Dusklight. Behaviour follows the decomp's src/dolphin/db/db.c and
// src/OdemuExi2/DebuggerDriver.c.
#include "../os/os_internal.h"

#include <dolphin/db.h>

#include <cstdlib>

using namespace cos_sdk::os;

extern "C" {
void DBInitInterrupts(void);
BOOL DBWrite(const void*, u32);
void DBOpen(void);
void DBClose(void);
}

namespace {

DBInterface sHostInterface = {}; // no debugger: bPresent 0, no exception marked

} // namespace

extern "C" {

DBInterface* __DBInterface = nullptr;

void DBInit(void) {
    Guard guard;
    __DBInterface = &sHostInterface;
    __DBInterface->ExceptionDestination = __DBExceptionDestination;
}

BOOL DBIsDebuggerPresent(void) {
    Guard guard;
    return (__DBInterface != nullptr && __DBInterface->bPresent != 0) ? TRUE : FALSE;
}

void DBPrintf(char* str, ...) {
    (void)str;
}

void __DBSetPresent(u32 value) {
    Guard guard;
    if (__DBInterface != nullptr) {
        __DBInterface->bPresent = value;
    }
}

void __DBMarkException(u8 exception, int value) {
    Guard guard;
    if (__DBInterface == nullptr || exception >= 32) {
        return;
    }
    const u32 mask = 1u << exception;
    if (value != 0) {
        __DBInterface->exceptionMask |= mask;
    } else {
        __DBInterface->exceptionMask &= ~mask;
    }
}

BOOL __DBIsExceptionMarked(__OSException exception) {
    Guard guard;
    if (__DBInterface == nullptr || exception >= 32) {
        return FALSE;
    }
    return (__DBInterface->exceptionMask & (1u << exception)) != 0 ? TRUE : FALSE;
}

void __DBExceptionDestinationAux(void) {
    Fatal("__DBExceptionDestination: an exception was routed to the debugger (the console dumps "
          "the context and halts)");
}

void __DBExceptionDestination(void) {
    __DBExceptionDestinationAux();
}

// ---- Debugger link (OdemuExi2) ----------------------------------------------------------------

void DBInitComm(int* inputFlagPtr, int* mtrCallback) {
    (void)inputFlagPtr;
    (void)mtrCallback;
    COS_SDK_LOG_ONCE("DBInitComm: there is no debugger link on the host (logged once)");
}

void DBInitInterrupts(void) {
    COS_SDK_LOG_ONCE("DBInitInterrupts: there is no debugger link on the host (logged once)");
}

s32 DBQueryData(void) {
    return 0; // nothing received
}

u32 DBRead(u8* buffer, u32 count) {
    (void)buffer;
    COS_SDK_LOG_ONCE("DBRead(%u bytes): there is no debugger link on the host; failing "
                     "(logged once)",
                     count);
    return 1;
}

BOOL DBWrite(const void* src, u32 size) {
    (void)src;
    COS_SDK_LOG_ONCE("DBWrite(%u bytes): there is no debugger link on the host; failing "
                     "(logged once)",
                     size);
    return 1;
}

void DBOpen(void) {}

void DBClose(void) {}

} // extern "C"
