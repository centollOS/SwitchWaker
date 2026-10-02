// Forwarder (phase 2, step 2.4): the game's dolphin/amcstubs/AmcExi2Stubs.h. Aurora has no AMC EXI2
// (debugger link) stubs, so every declaration is the decomp's. Nothing on the host defines them:
// their only users (amcstubs, TRK, OdemuExi2) are outside the native build.
#ifndef COS_SDK_DOLPHIN_AMCSTUBS_AMCEXI2STUBS_H
#define COS_SDK_DOLPHIN_AMCSTUBS_AMCEXI2STUBS_H

#include <dolphin/os/OS.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __OSInterruptHandler AmcEXICallback;

typedef enum {
    AMC_EXI_NO_ERROR = 0,
    AMC_EXI_UNSELECTED
} AmcExiError;

void EXI2_Init(vu8**, AmcEXICallback);
void EXI2_EnableInterrupts(void);
int EXI2_Poll(void);
AmcExiError EXI2_ReadN(void*, u32);
AmcExiError EXI2_WriteN(const void*, u32);
void EXI2_Reserve(void);
void EXI2_Unreserve(void);
BOOL AMC_IsStub(void);

#ifdef __cplusplus
}
#endif

#endif
