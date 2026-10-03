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
//
// Added in step 2.7 (JSystem-core): the decomp's OSAlloc.h rounding macros (OSRoundUp, OSRoundDown
// and their Ptr forms, through uintptr_t as the decomp's TARGET_PC branch), its OSError.h names
// Aurora spells differently or lacks (OS_ERROR_MEMORY_PROTECTION, OS_ERROR_FLOATING_POINT_EXCEPTION,
// the OSException enum), and OSContextPPC, a view of the PowerPC register image for code that reads
// OSContext fields: Aurora's TARGET_PC OSContext is opaque storage of the same size.
//
// Added in step 2.7 (m_Do): __OSActiveThreadQueue, the active-thread list the game's OSThread.h names
// OS_THREAD_QUEUE; cos_sdk defines it.
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

// The SDK's active-thread list, which the game's dolphin/os/OSThread.h declares as OS_THREAD_QUEUE (at
// 0x800000DC). Aurora declares it only for __MWERKS__; cos_sdk defines it (os/OSThread.cpp) and
// keeps it under the OS lock, so walk it with interrupts disabled, as the SDK does.
extern OSThreadQueue __OSActiveThreadQueue;

// The decomp's name for Aurora's __OSExceptionHandler.
typedef __OSExceptionHandler OSExceptionHandler;

#define OS_ASSERT(...)

// dolphin/os/OSAlloc.h (the decomp's TARGET_PC branch).
#define OSRoundUp(x, align) (((x) + (align)-1) & (-(align)))
#define OSRoundUpPtr(x, align) ((void*)((((uintptr_t)(x)) + (align)-1) & (~((align)-1))))
#define OSRoundDown(x, align) ((x) & (-(align)))
#define OSRoundDownPtr(x, align) ((void*)(((uintptr_t)(x)) & (~((align)-1))))

// dolphin/os/OSError.h: Aurora calls 15 OS_ERROR_PROTECTION and has no name for 16, although its
// __OSErrorTable has 17 entries.
#define OS_ERROR_MEMORY_PROTECTION OS_ERROR_PROTECTION
#define OS_ERROR_FLOATING_POINT_EXCEPTION 16

typedef enum {
    EXCEPTION_SYSTEM_RESET,
    EXCEPTION_MACHINE_CHECK,
    EXCEPTION_DSI,
    EXCEPTION_ISI,
    EXCEPTION_EXTERNAL_INTERRUPT,
    EXCEPTION_ALIGNMENT,
    EXCEPTION_PROGRAM,
    EXCEPTION_FLOATING_POINT,
    EXCEPTION_DECREMENTER,
    EXCEPTION_SYSTEM_CALL,
    EXCEPTION_TRACE,
    EXCEPTION_PERFORMANCE_MONITOR,
    EXCEPTION_BREAKPOINT,
    EXCEPTION_RESERVED,
    EXCEPTION_THERMAL_INTERRUPT,
    EXCEPTION_MEMORY_PROTECTION,
    EXCEPTION_FLOATING_POINT_EXCEPTION,
    EXCEPTION_MAX,
} OSException;

// The PowerPC register image of the decomp's OSContext (dolphin/os/OSContext.h, offsets in the
// comments). Aurora's TARGET_PC OSContext is `char storage[0x2C8]`, the same size, so game code that
// reads or writes register fields (JUTException's crash report) goes through this view. Nothing on
// the host fills it: cos_sdk never raises a CPU exception and saves no registers into a context.
typedef struct OSContextPPC {
    /* 0x000 */ u32 gpr[32];
    /* 0x080 */ u32 cr;
    /* 0x084 */ u32 lr;
    /* 0x088 */ u32 ctr;
    /* 0x08C */ u32 xer;
    /* 0x090 */ f64 fpr[32];
    /* 0x190 */ u32 fpscr_pad;
    /* 0x194 */ u32 fpscr;
    /* 0x198 */ u32 srr0;
    /* 0x19C */ u32 srr1;
    /* 0x1A0 */ u16 mode;
    /* 0x1A2 */ u16 state;
    /* 0x1A4 */ u32 gqr[8];
    /* 0x1C4 */ u32 psf_pad;
    /* 0x1C8 */ f64 psf[32];
} OSContextPPC;

#ifdef __cplusplus
static_assert(sizeof(OSContextPPC) == sizeof(OSContext), "OSContextPPC must match OSContext's storage");
#else
_Static_assert(sizeof(OSContextPPC) == sizeof(OSContext), "OSContextPPC must match OSContext's storage");
#endif

static inline OSContextPPC* OSContextPPCOf(OSContext* context) {
    return (OSContextPPC*)context;
}

#ifdef __cplusplus
}
#endif

#endif
