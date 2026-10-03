// cos_sdk: the SDK-internal SRAM and RTC functions (docs/NATIVE_PORT_PHASE2_3.md, step 2.6b).
//
// The GameCube SDK declares these in its private headers; Aurora's <dolphin/os/OSRtc.h> has the
// OSSram/OSSramEx structures but not the functions. cos_sdk implements them over an in-memory SRAM
// (src/os/OSSram.cpp); the public getters and setters (OSGetSoundMode, OSSetProgressiveMode,
// OSGetLanguage, ...) go through them, as in the SDK.
#ifndef COS_SDK_SRAM_H
#define COS_SDK_SRAM_H

#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

// Locks the SRAM copy and returns it, with interrupts disabled until the matching unlock (as in
// the SDK). Returns NULL if it is already locked. __OSUnlockSram*(TRUE) commits the change: the
// checksums of OSSram are recomputed and the copy counts as written back (nothing is stored on the
// host: the SRAM starts from its defaults in every process). Both return the sync state.
OSSram* __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
BOOL __OSUnlockSram(BOOL commit);
BOOL __OSUnlockSramEx(BOOL commit);
BOOL __OSSyncSram(void);
// Whether the OSSram checksums are valid.
BOOL __OSCheckSram(void);

// The real-time clock: seconds since 2000-01-01 00:00:00 (the GameCube epoch), from the host's
// wall clock. __OSSetRTC does not change the host clock: it records an offset that later
// __OSGetRTC calls add (it is logged). Both return TRUE.
BOOL __OSGetRTC(u32* rtc);
BOOL __OSSetRTC(u32 rtc);

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_SRAM_H
