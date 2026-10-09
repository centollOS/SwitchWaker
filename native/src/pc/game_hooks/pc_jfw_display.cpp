// The host body of JFWDisplay.cpp's waitForTick (the frame pacing), moved out of game/ (step G3
// of docs/GAME_CODE_ORGANIZATION.md). Declared in native/include/pc/game_hooks.h.
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "dolphin/os/OS.h"
#include "dolphin/vi.h"

void pc_wait_for_tick(u32 p1, u16 p2) {
    // Step 6.2 (docs/NATIVE_PORT_PHASE4_6.md). The console sleeps here until p1 ticks have passed
    // since the last wait (tick-rate mode, an OSAlarm) or until the VI interrupt has counted p2
    // more retraces (frame-rate mode, JUTVideo's post-retrace message). The host has no VI
    // interrupt: pc_frame_pace (Dusklight's limiter, skipped with COS_UNCAPPED) waits out the same
    // period, and the retraces the console would have counted during the wait happen here, so
    // JUTVideo's retrace callbacks show the drawn XFB before this frame's exchange and the retrace
    // count advances as on the console (one per frame at 60 fps, two at 30). Retraces made since
    // the last wait by code that waits on VIWaitForRetrace (JKRDvdRipper) count towards them.
    // JUTVideo's message queue is no longer read: only this wait read it.
    static u32 nextCount = VIGetRetraceCount();
    unsigned long long periodNs;
    u32 retraces;
    if (p1 != 0) {
        periodNs = (unsigned long long)p1 * 1000000000ull / OS_TIMER_CLOCK;
        retraces = (u32)((periodNs + PC_RETRACE_PERIOD_NS / 2) / PC_RETRACE_PERIOD_NS);
        if (retraces == 0) {
            retraces = 1;
        }
    } else {
        retraces = (p2 == 0) ? 1 : p2;
        periodNs = retraces * PC_RETRACE_PERIOD_NS;
    }
    // COS_FPS60_TEST (pc_frame_split): a game frame of two retraces is painted twice, each paint
    // waiting one retrace, so the game still runs at its speed.
    const u32 waitRetraces = pc_frame_wait_retraces(retraces);
    if (waitRetraces == 0) {
        return; // COS_PAINT_PURITY_REPEAT's extra paint: no wait
    }
    if (waitRetraces != retraces) {
        retraces = waitRetraces;
        periodNs = retraces * PC_RETRACE_PERIOD_NS;
    }
    pc_frame_pace(periodNs);
    while ((s32)(VIGetRetraceCount() - nextCount) < 0) {
        VIWaitForRetrace();
    }
    nextCount = VIGetRetraceCount() + retraces;
}
