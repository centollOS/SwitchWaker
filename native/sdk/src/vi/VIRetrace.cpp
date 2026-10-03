// cos_sdk: VI retrace emulation and the VI state Aurora at 3227d76 does not keep
// (docs/NATIVE_PORT_PHASE2_3.md, step 2.6c): VIWaitForRetrace, the retrace count, the pre/post
// retrace callbacks, the next/current frame buffer, VISetBlack, VIGetNextField and VIGetDTVStatus.
// Aurora's vi.cpp provides the rest (VIInit, VIConfigure, VIFlush, VIGetTvFormat, window control).
//
// Provenance: adapted from the "VI" section of Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight):
// on the GameCube the VI raises an interrupt at every retrace (~60 Hz) whose handler bumps the
// retrace count and calls the pre- and post-retrace callbacks (JUTVideo's, which pace the game);
// with no VI on the host, each VIWaitForRetrace call is one retrace. Changed:
// - the retrace runs as the SDK's handler does (vi.c __VIRetraceHandler): count, pre-retrace
//   callback, frame buffer latch, post-retrace callback, all with interrupts disabled (the OS lock
//   of src/os, so it is ordered with OSDisableInterrupts sections and alarm handlers on other
//   threads); Dusklight runs the callbacks unguarded;
// - VISetNextFrameBuffer and VISetBlack keep their values: VIGetNextFrameBuffer returns the
//   buffer, VIGetCurrentFrameBuffer returns it from the next retrace on, COSSdkVIIsBlack
//   (cos_sdk/hooks.h) gives the blanking to the game glue (Dusklight logs and drops them);
// - VIGetNextField alternates with the retrace count instead of always returning 0, so code that
//   waits for a given field (d_a_movie_player with field-based THP video) sees it within two
//   retraces;
// - VISetBlack(TRUE) is logged once: Aurora keeps presenting frames, it does not blank the screen.
//
// Pacing is not done here: VIWaitForRetrace returns at once. The main loop paces frames
// (phase 6); a thread that polls with VIWaitForRetrace (JKRDvdRipper while a read is pending)
// spins and produces retraces as it goes.
#include "../os/os_internal.h"

#include <dolphin/vi.h>

#include "cos_sdk/hooks.h"

#include <atomic>

using namespace cos_sdk::os;

namespace {

std::atomic<u32> sRetraceCount{0}; // written with Lock() held, read without it
VIRetraceCallback sPreRetraceCallback = nullptr;  // guarded by Lock()
VIRetraceCallback sPostRetraceCallback = nullptr; // guarded by Lock()
void* sNextFrameBuffer = nullptr;                 // guarded by Lock()
void* sCurrentFrameBuffer = nullptr;              // guarded by Lock()
bool sFrameBufferSet = false;                     // guarded by Lock(); latched at the next retrace
BOOL sBlack = FALSE;                              // guarded by Lock()

} // namespace

extern "C" {

void VIWaitForRetrace(void) {
    // On the console the caller sleeps until the retrace interrupt; here the call is the retrace.
    Guard guard;
    const u32 count = sRetraceCount.load(std::memory_order_relaxed) + 1;
    sRetraceCount.store(count, std::memory_order_release);
    if (sPreRetraceCallback != nullptr) {
        sPreRetraceCallback(count);
    }
    if (sFrameBufferSet) {
        sCurrentFrameBuffer = sNextFrameBuffer;
        sFrameBufferSet = false;
    }
    if (sPostRetraceCallback != nullptr) {
        sPostRetraceCallback(count);
    }
}

u32 VIGetRetraceCount(void) {
    return sRetraceCount.load(std::memory_order_acquire);
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
    Guard guard;
    const VIRetraceCallback previous = sPreRetraceCallback;
    sPreRetraceCallback = cb;
    return previous;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
    Guard guard;
    const VIRetraceCallback previous = sPostRetraceCallback;
    sPostRetraceCallback = cb;
    return previous;
}

void VISetNextFrameBuffer(void* fb) {
    if ((reinterpret_cast<uintptr_t>(fb) & 0x1F) != 0) {
        // The SDK asserts this (vi.c): the VI fetches the XFB in 32-byte units.
        Log("VISetNextFrameBuffer: frame buffer %p is not 32-byte aligned", fb);
    }
    Guard guard;
    sNextFrameBuffer = fb;
    sFrameBufferSet = true;
}

void* VIGetNextFrameBuffer(void) {
    Guard guard;
    return sNextFrameBuffer;
}

void* VIGetCurrentFrameBuffer(void) {
    Guard guard;
    return sCurrentFrameBuffer;
}

void VISetBlack(BOOL black) {
    if (black) {
        COS_SDK_LOG_ONCE("VISetBlack(TRUE): recorded (COSSdkVIIsBlack), but Aurora keeps "
                         "presenting frames; the screen is not blanked");
    }
    Guard guard;
    sBlack = black;
}

BOOL COSSdkVIIsBlack(void) {
    Guard guard;
    return sBlack;
}

u32 VIGetNextField(void) {
    // Interlaced output alternates fields every retrace; the field of retrace n is n & 1. The host
    // presents whole frames, so this only keeps field-waiting code moving.
    return (sRetraceCount.load(std::memory_order_acquire) + 1) & 1;
}

u32 VIGetDTVStatus(void) {
    // Whether a component (progressive-capable) cable is plugged in. 0, as in Dusklight: the
    // logo scene (d_s_logo.cpp) then never offers progressive scan, and the game keeps the render
    // mode it starts with; Aurora presents whole frames either way.
    return 0;
}

} // extern "C"
