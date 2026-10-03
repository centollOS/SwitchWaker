// cos_sdk: the small OS functions with no device behind them (docs/NATIVE_PORT_PHASE2_3.md,
// step 2.6b): console type and memory size, the bus clock, OSFatal, error handlers, memory
// protection, stopwatches and the IPL font.
//
// Provenance: adapted from Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight), "General OS" and
// "Remaining OS Stubs": OSGetConsoleType returns OS_CONSOLE_RETAIL1, OSInitFont returns FALSE,
// OSGetFontEncode returns 0 (ANSI), OSProtectRange does nothing. Changed:
// - OSSetErrorHandler records the handler and returns the previous one (Dusklight returns NULL and
//   drops it); COSSdkGetErrorHandler hands it to the game glue;
// - OSFatal aborts loudly (Dusklight leaves it to Aurora, whose OSReport.cpp is compiled out);
// - OSProtectRange logs once when asked for a protection the host cannot give;
// - OSGetFontTexture/Width/Texel abort: without a ROM font (OSInitFont fails) no caller may reach
//   them, and Dusklight's NULL results would be dereferenced;
// - added: OSGetConsoleSimulatedMemSize, the __OSBusClock variable, __OSFpscrEnableBits, the
//   stopwatch functions (the SDK's algorithm, timed with OSGetTime), OSSetFontEncode, OSLoadFont,
//   OSSetFontWidth.
#include "os_internal.h"

#include <dolphin/base/PPCArch.h>

#include "cos_sdk/hooks.h"

#include <cinttypes>

using namespace cos_sdk::os;

// Aurora's <dolphin/os.h> turns __OSBusClock into a read of low memory (MEM1 + 0xF8). The decomp's
// headers (phase 1's COS_SDK_HEADERS=decomp, removed in step 2.8) declared it as
// `extern u32 __OSBusClock` and derived OS_BUS_CLOCK and OS_TIMER_CLOCK from it; the variable keeps
// the console's value for any unit that still names it. Aurora's own OS_BUS_CLOCK has the same value.
#undef __OSBusClock

namespace {

// The retail console's main memory: 24 MiB. m_Do_machine.cpp shrinks its arena on 48 MiB
// development units (OSGetConsoleSimulatedMemSize() >= 0x3000000); the host runs as a retail unit.
constexpr u32 kRetailMemSize = 0x01800000;

u16 sFontEncode = OS_FONT_ENCODE_ANSI; // guarded by Lock()
int sFontFixedWidth = FALSE;           // guarded by Lock()

} // namespace

extern "C" {

u32 __OSBusClock = OS_BUS_CLOCK;

// The SDK's default (OSError.c): every floating-point exception enabled once a handler for
// OS_ERROR_FLOATING_POINT is installed. JUTException.cpp sets it. No host FP exception traps.
u32 __OSFpscrEnableBits = FPSCR_VE | FPSCR_OE | FPSCR_UE | FPSCR_ZE | FPSCR_XE;

OSErrorHandler __OSErrorTable[17] = {};

u32 OSGetConsoleType(void) {
    return OS_CONSOLE_RETAIL1;
}

u32 OSGetConsoleSimulatedMemSize(void) {
    return kRetailMemSize;
}

void OSFatal(GXColor fg, GXColor bg, const char* msg) {
    (void)fg;
    (void)bg;
    Fatal("OSFatal: %s", msg != nullptr ? msg : "(null)");
}

// ---- Error handlers ---------------------------------------------------------------------------

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    if (error >= sizeof(__OSErrorTable) / sizeof(__OSErrorTable[0])) {
        Fatal("OSSetErrorHandler: error %u out of range", static_cast<unsigned>(error));
    }
    if (handler != nullptr) {
        COS_SDK_LOG_ONCE("OSSetErrorHandler: handlers are recorded, but no host fault reaches "
                         "them yet (the game glue calls them through COSSdkGetErrorHandler)");
    }
    Guard guard;
    const OSErrorHandler old = __OSErrorTable[error];
    __OSErrorTable[error] = handler;
    return old;
}

OSErrorHandler COSSdkGetErrorHandler(OSError error) {
    if (error >= sizeof(__OSErrorTable) / sizeof(__OSErrorTable[0])) {
        return nullptr;
    }
    Guard guard;
    return __OSErrorTable[error];
}

// ---- Memory protection ------------------------------------------------------------------------

void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    if (chan > OS_PROTECT_CHAN3) {
        Fatal("OSProtectRange: channel %u out of range", static_cast<unsigned>(chan));
    }
    // OS_PROTECT_CONTROL_RDWR (what JUTException asks for) allows everything, as the host does.
    if ((control & OS_PROTECT_CONTROL_RDWR) != OS_PROTECT_CONTROL_RDWR && nBytes != 0) {
        COS_SDK_LOG_ONCE("OSProtectRange(%u, %p, 0x%x, %u): memory protection is not "
                         "available on the host; the range stays readable and writable",
                         static_cast<unsigned>(chan), addr, static_cast<unsigned>(nBytes),
                         static_cast<unsigned>(control));
    }
}

// ---- Stopwatches (the SDK's OSStopwatch.c algorithm) ------------------------------------------

void OSInitStopwatch(OSStopwatch* sw, char* name) {
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFLL;
    sw->max = 0;
    sw->running = FALSE;
}

void OSStartStopwatch(OSStopwatch* sw) {
    sw->running = TRUE;
    sw->last = OSGetTime();
}

void OSStopStopwatch(OSStopwatch* sw) {
    if (!sw->running) {
        return;
    }
    const OSTime interval = OSGetTime() - sw->last;
    sw->total += interval;
    sw->running = FALSE;
    sw->hits++;
    if (sw->max < interval) {
        sw->max = interval;
    }
    if (interval < sw->min) {
        sw->min = interval;
    }
}

OSTime OSCheckStopwatch(OSStopwatch* sw) {
    OSTime total = sw->total;
    if (sw->running) {
        total += OSGetTime() - sw->last;
    }
    return total;
}

void OSResetStopwatch(OSStopwatch* sw) {
    OSInitStopwatch(sw, sw->name);
}

void OSDumpStopwatch(OSStopwatch* sw) {
    OSReport("Stopwatch [%s]\t:\n", sw->name != nullptr ? sw->name : "");
    OSReport("\tTotal= %" PRId64 " us\n", static_cast<int64_t>(OSTicksToMicroseconds(sw->total)));
    OSReport("\tHits = %u \n", static_cast<unsigned>(sw->hits));
    OSReport("\tMin  = %" PRId64 " us\n", static_cast<int64_t>(OSTicksToMicroseconds(sw->min)));
    OSReport("\tMax  = %" PRId64 " us\n", static_cast<int64_t>(OSTicksToMicroseconds(sw->max)));
    const OSTime mean = sw->hits != 0 ? sw->total / sw->hits : 0;
    OSReport("\tMean = %" PRId64 " us\n", static_cast<int64_t>(OSTicksToMicroseconds(mean)));
}

// ---- IPL font: there is no boot ROM, so no font ------------------------------------------------

u16 OSGetFontEncode(void) {
    Guard guard;
    return sFontEncode;
}

u16 OSSetFontEncode(u16 encode) {
    Guard guard;
    const u16 old = sFontEncode;
    if (encode <= OS_FONT_ENCODE_MAX) {
        sFontEncode = encode;
    }
    return old;
}

BOOL OSInitFont(OSFontHeader* fontData) {
    (void)fontData;
    COS_SDK_LOG_ONCE("OSInitFont: there is no IPL font ROM on the host; returning FALSE");
    return FALSE;
}

u32 OSLoadFont(OSFontHeader* fontData, void* tmp) {
    (void)fontData;
    (void)tmp;
    COS_SDK_LOG_ONCE("OSLoadFont: there is no IPL font ROM on the host; returning 0");
    return 0;
}

char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width) {
    (void)string;
    (void)image;
    (void)x;
    (void)y;
    (void)width;
    Fatal("OSGetFontTexture: no IPL font is loaded (OSInitFont always fails on the host)");
}

char* OSGetFontWidth(const char* string, s32* width) {
    (void)string;
    (void)width;
    Fatal("OSGetFontWidth: no IPL font is loaded (OSInitFont always fails on the host)");
}

char* OSGetFontTexel(const char* string, void* image, s32 pos, s32 stride, s32* width) {
    (void)string;
    (void)image;
    (void)pos;
    (void)stride;
    (void)width;
    Fatal("OSGetFontTexel: no IPL font is loaded (OSInitFont always fails on the host)");
}

int OSSetFontWidth(int fixed) {
    Guard guard;
    const int old = sFontFixedWidth;
    sFontFixedWidth = fixed;
    return old;
}

} // extern "C"
