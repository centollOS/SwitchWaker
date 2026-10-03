// cos_sdk: the amcstubs library (docs/NATIVE_PORT_PHASE2_3.md, step 2.6e). Aurora has none.
//
// amcstubs is the SDK's own stand-in for the AMC DDH debugger's EXI2 link, linked into retail
// builds: every function does nothing and AMC_IsStub returns 1, so TRK falls back to the other
// debugger link (TRK_MINNOW_DOLPHIN's dolphin_trk_glue.c). That is exactly right for the host,
// which has no AMC hardware either, so this is the same code with nothing changed in behaviour.
// The declarations are the game's (native/include/sdk/dolphin/amcstubs/AmcExi2Stubs.h), repeated here
// because cos_sdk compiles against Aurora's headers only (sdk.cmake).
//
// Provenance: not in Dusklight. Copied from the decomp's src/amcstubs/AmcExi2Stubs.c (in this
// repository: game/src/amcstubs/AmcExi2Stubs.c, which the native build does not compile);
// only the parameter types follow the forwarder header (vu8**, u32).
#include <dolphin/os.h>

extern "C" {

typedef __OSInterruptHandler AmcEXICallback;

typedef enum {
    AMC_EXI_NO_ERROR = 0,
    AMC_EXI_UNSELECTED
} AmcExiError;

void EXI2_Init(vu8** inputPendingPtrRef, AmcEXICallback monitorCallback) {
    (void)inputPendingPtrRef;
    (void)monitorCallback;
}

void EXI2_EnableInterrupts(void) {}

int EXI2_Poll(void) {
    return 0;
}

AmcExiError EXI2_ReadN(void* bytes, u32 length) {
    (void)bytes;
    (void)length;
    return AMC_EXI_NO_ERROR;
}

AmcExiError EXI2_WriteN(const void* bytes, u32 length) {
    (void)bytes;
    (void)length;
    return AMC_EXI_NO_ERROR;
}

void EXI2_Reserve(void) {}

void EXI2_Unreserve(void) {}

BOOL AMC_IsStub(void) {
    return 1;
}

} // extern "C"
