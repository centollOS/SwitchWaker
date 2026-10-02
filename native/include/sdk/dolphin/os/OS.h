// Forwarder (phase 2, step 2.3): the game's dolphin/os/OS.h over Aurora's <dolphin/os.h>.
//
// Brings in what the decomp's OS.h brought in (PPCArch, DVD, every OS sub-header) and adds the
// game-only declarations Aurora lacks. Left out on purpose:
// - the hardware registers and low-memory globals the decomp defines in the header at fixed
//   addresses (__OSDeviceCode, OS_PI_INTR_*, OS_DSP_*, OS_ARAM_*, BOOT_REGION_*,
//   __gUnknown800030E3): no host meaning, and a definition in every unit;
// - file-static and exception-vector internals of the SDK's own source (InquiryCallback,
//   OSExceptionInit, OSExceptionVector, OSDefaultExceptionHandler, __OSDBIntegrator, __OSDBJump,
//   __DBVECTOR, __OSEVSetNumber, __OSEVEnd);
// - BI2Debug and GLOBAL_MEMORY, the GameCube low-memory layout;
// - declarations Aurora has with another signature (OSGetStackPointer returns u32 there; OSBootInfo
//   has other field names): Aurora's wins and the game adapts under TARGET_PC (step 2.7).
#ifndef COS_SDK_DOLPHIN_OS_OS_H
#define COS_SDK_DOLPHIN_OS_OS_H

#include <stdarg.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include "cos_sdk_extras.h"

#include <dolphin/os/OSArena.h>
#include <dolphin/os/OSAudioSystem.h>
#include <dolphin/os/OSLink.h>
#include <dolphin/os/OSStopwatch.h>
#include <dolphin/os/OSSync.h>
#include <dolphin/os/OSUtil.h>

#ifdef __cplusplus
extern "C" {
#endif

// OSReport switches, defined by the game (m_Do_printf.cpp).
extern u8 __OSReport_disable;
extern u8 __OSReport_Error_disable;
extern u8 __OSReport_Warning_disable;
extern u8 __OSReport_System_disable;
extern u8 __OSReport_enable;

// The decomp's name for Aurora's __OSExceptionHandler.
typedef __OSExceptionHandler OSExceptionHandler;

#define OS_ASSERT(...)

#ifdef __cplusplus
}
#endif

#endif
