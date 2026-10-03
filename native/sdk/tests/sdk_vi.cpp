// cos_sdk_smoke: the "vi" and "gx" tests of step 2.6c (VI retrace emulation, src/vi/VIRetrace.cpp;
// the GX functions Aurora lacks, src/gx/GXExtras.cpp).
#include "smoke.h"

#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <dolphin/vi.h>

#include "cos_sdk/gx.h"
#include "cos_sdk/hooks.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>

namespace {

// ---- vi ------------------------------------------------------------------------------------------

// Written by the retrace callbacks; they run with interrupts disabled, so plain variables guarded
// by OSDisableInterrupts are enough.
u32 sPreCalls = 0;
u32 sPostCalls = 0;
u32 sPreLastCount = 0;
u32 sPostLastCount = 0;
bool sCallbackHadInterruptsOn = false;
bool sPostSawLatchedBuffer = true;
void* sExpectedBuffer = nullptr;

void PreRetrace(u32 retraceCount) {
    // Interrupts are already disabled inside the retrace "interrupt": Disable returns FALSE.
    if (OSDisableInterrupts()) {
        sCallbackHadInterruptsOn = true;
        OSEnableInterrupts();
    }
    sPreCalls++;
    sPreLastCount = retraceCount;
}

void PostRetrace(u32 retraceCount) {
    sPostCalls++;
    sPostLastCount = retraceCount;
    if (sExpectedBuffer != nullptr && VIGetCurrentFrameBuffer() != sExpectedBuffer) {
        sPostSawLatchedBuffer = false;
    }
}

void OtherPreRetrace(u32) {}

// ---- gx ------------------------------------------------------------------------------------------

u16 sSyncToken = 0;
int sSyncCalls = 0;
bool sSyncHadInterruptsOn = false;
GXDrawSyncCallback sSyncOldCallback = nullptr;

void SyncCallback(u16 token) {
    if (OSDisableInterrupts()) {
        sSyncHadInterruptsOn = true;
        OSEnableInterrupts();
    }
    sSyncToken = token;
    sSyncCalls++;
}

// As m_Do_graphic's capture callback: puts the previous callback back from inside the callback.
void SelfRemovingSyncCallback(u16 token) {
    const BOOL level = OSDisableInterrupts();
    const GXDrawSyncCallback mine = GXSetDrawSyncCallback(sSyncOldCallback);
    if (mine == &SelfRemovingSyncCallback) {
        sSyncToken = token;
        sSyncCalls++;
    }
    OSRestoreInterrupts(level);
}

} // namespace

COS_SMOKE_TEST(vi) {
    const u32 start = VIGetRetraceCount();

    // The plan's check: a pre-retrace callback, VIWaitForRetrace() three times, three retraces.
    COS_SMOKE_CHECK(VISetPreRetraceCallback(PreRetrace) == nullptr);
    COS_SMOKE_CHECK(VISetPostRetraceCallback(PostRetrace) == nullptr);
    for (int i = 0; i < 3; i++) {
        VIWaitForRetrace();
    }
    COS_SMOKE_CHECK(VIGetRetraceCount() - start == 3);
    COS_SMOKE_CHECK(sPreCalls == 3);
    COS_SMOKE_CHECK(sPostCalls == 3);
    COS_SMOKE_CHECK(sPreLastCount == start + 3);
    COS_SMOKE_CHECK(sPostLastCount == start + 3);
    COS_SMOKE_CHECK(!sCallbackHadInterruptsOn);

    // Setting a callback returns the previous one.
    COS_SMOKE_CHECK(VISetPreRetraceCallback(OtherPreRetrace) == PreRetrace);
    COS_SMOKE_CHECK(VISetPreRetraceCallback(PreRetrace) == OtherPreRetrace);

    // The next frame buffer becomes the current one at the next retrace, before the post-retrace
    // callback runs.
    alignas(32) static u8 xfbA[64];
    alignas(32) static u8 xfbB[64];
    VISetNextFrameBuffer(xfbA);
    VIWaitForRetrace();
    COS_SMOKE_CHECK(VIGetCurrentFrameBuffer() == xfbA);
    VISetNextFrameBuffer(xfbB);
    COS_SMOKE_CHECK(VIGetNextFrameBuffer() == xfbB);
    COS_SMOKE_CHECK(VIGetCurrentFrameBuffer() == xfbA);
    sExpectedBuffer = xfbB;
    VIWaitForRetrace();
    sExpectedBuffer = nullptr;
    COS_SMOKE_CHECK(sPostSawLatchedBuffer);
    COS_SMOKE_CHECK(VIGetCurrentFrameBuffer() == xfbB);

    // The next field alternates with each retrace.
    const u32 field = VIGetNextField();
    COS_SMOKE_CHECK(field <= 1);
    VIWaitForRetrace();
    COS_SMOKE_CHECK(VIGetNextField() == (field ^ 1));
    VIWaitForRetrace();
    COS_SMOKE_CHECK(VIGetNextField() == field);

    // Blanking is recorded for the game glue.
    COS_SMOKE_CHECK(!COSSdkVIIsBlack());
    VISetBlack(TRUE);
    COS_SMOKE_CHECK(COSSdkVIIsBlack());
    VISetBlack(FALSE);
    COS_SMOKE_CHECK(!COSSdkVIIsBlack());

    COS_SMOKE_CHECK(VIGetDTVStatus() == 0);

    // Retraces from several threads are serialised: none is lost, every callback sees its own
    // count.
    const u32 before = VIGetRetraceCount();
    const u32 preBefore = sPreCalls;
    std::thread other([] {
        for (int i = 0; i < 500; i++) {
            VIWaitForRetrace();
        }
    });
    for (int i = 0; i < 500; i++) {
        VIWaitForRetrace();
        (void)VIGetRetraceCount();
    }
    other.join();
    COS_SMOKE_CHECK(VIGetRetraceCount() - before == 1000);
    {
        const BOOL level = OSDisableInterrupts();
        const u32 preCalls = sPreCalls;
        const u32 lastCount = sPreLastCount;
        OSRestoreInterrupts(level);
        COS_SMOKE_CHECK(preCalls - preBefore == 1000);
        COS_SMOKE_CHECK(lastCount == VIGetRetraceCount());
    }

    COS_SMOKE_CHECK(VISetPreRetraceCallback(nullptr) == PreRetrace);
    COS_SMOKE_CHECK(VISetPostRetraceCallback(nullptr) == PostRetrace);
    VIWaitForRetrace(); // no callbacks: still a retrace
    COS_SMOKE_CHECK(VIGetRetraceCount() - before == 1001);
    return true;
}

COS_SMOKE_TEST(gx) {
    // Draw sync: the callback gets the token, with interrupts disabled, and is returned as the
    // previous callback.
    COS_SMOKE_CHECK(GXSetDrawSyncCallback(SyncCallback) == nullptr);
    GXSetDrawSync(0x1234);
    COS_SMOKE_CHECK(sSyncCalls == 1);
    COS_SMOKE_CHECK(sSyncToken == 0x1234);
    COS_SMOKE_CHECK(!sSyncHadInterruptsOn);
    // A callback that puts the previous one back from inside itself (m_Do_graphic's capture).
    sSyncOldCallback = GXSetDrawSyncCallback(SelfRemovingSyncCallback);
    COS_SMOKE_CHECK(sSyncOldCallback == SyncCallback);
    GXSetDrawSync(0);
    COS_SMOKE_CHECK(sSyncCalls == 2);
    COS_SMOKE_CHECK(sSyncToken == 0);
    COS_SMOKE_CHECK(GXSetDrawSyncCallback(nullptr) == SyncCallback);
    GXSetDrawSync(7); // no callback
    COS_SMOKE_CHECK(sSyncCalls == 2);

    // XFB arithmetic: the SDK's values (GXFrameBuf.c; Dusklight's SDK has the same algorithm). NTSC
    // (GXNtsc480IntDf: EFB 480, XFB 480) is 1:1. For PAL (GXPal528IntDf: EFB 528, XFB 574) the SDK
    // settles on the scale 572/528, which gives 572 lines: GXGetYScaleFactor returns the last
    // scale that stays below the requested height when no scale hits it exactly.
    COS_SMOKE_CHECK(GXGetYScaleFactor(480, 480) == 1.0f);
    COS_SMOKE_CHECK(GXGetNumXfbLines(480, 1.0f) == 480);
    const f32 pal = GXGetYScaleFactor(528, 574);
    COS_SMOKE_CHECK(pal == 572.0f / 528.0f);
    COS_SMOKE_CHECK(GXGetNumXfbLines(528, pal) == 572);
    COS_SMOKE_CHECK(GXGetNumXfbLines(480, GXGetYScaleFactor(480, 574)) == 574);
    // __GXGetNumXfbLines caps at 1024 lines.
    COS_SMOKE_CHECK(GXGetNumXfbLines(1000, 2.0f) == 0x400);
    // The scale never gives more lines than asked for, so a buffer of that height is big enough.
    for (u16 efb = 400; efb <= 528; efb++) {
        for (u16 xfb = efb; xfb <= 600; xfb++) {
            COS_SMOKE_CHECK(GXGetNumXfbLines(efb, GXGetYScaleFactor(efb, xfb)) <= xfb);
        }
    }

    // The GX thread: the default (main) thread until someone takes it.
    OSThread* const mainThread = OSGetCurrentThread();
    COS_SMOKE_CHECK(GXGetCurrentGXThread() == mainThread);
    OSThread* otherThread = nullptr;
    OSThread* previousSeen = nullptr;
    OSThread* currentSeen = nullptr;
    std::thread other([&] {
        otherThread = OSGetCurrentThread();
        previousSeen = GXSetCurrentGXThread();
        currentSeen = GXGetCurrentGXThread();
    });
    other.join();
    COS_SMOKE_CHECK(previousSeen == mainThread);
    COS_SMOKE_CHECK(currentSeen == otherThread);
    COS_SMOKE_CHECK(GXGetCurrentGXThread() == otherThread);
    COS_SMOKE_CHECK(GXSetCurrentGXThread() == otherThread);
    COS_SMOKE_CHECK(GXGetCurrentGXThread() == mainThread);

    // Texture cache regions, as J3DSys sets them up (32 KiB even and odd caches).
    GXTexRegion region;
    std::memset(&region, 0xCD, sizeof(region));
    GXInitTexCacheRegion(&region, GX_FALSE, 0x80000, GX_TEXCACHE_32K, 0x0, GX_TEXCACHE_32K);
    u32 words[4];
    std::memcpy(words, &region, sizeof(words));
    COS_SMOKE_CHECK(words[0] == ((0x80000u >> 5) | (3u << 15) | (3u << 18)));
    COS_SMOKE_CHECK(words[1] == ((3u << 15) | (3u << 18)));
    u8 flags[2];
    std::memcpy(flags, reinterpret_cast<const u8*>(&region) + 12, sizeof(flags));
    COS_SMOKE_CHECK(flags[0] == 0 && flags[1] == 1);
    GXInitTexCacheRegion(&region, GX_TRUE, 0x8000, GX_TEXCACHE_128K, 0x88000, GX_TEXCACHE_NONE);
    std::memcpy(words, &region, sizeof(words));
    COS_SMOKE_CHECK(words[0] == ((0x8000u >> 5) | (4u << 15) | (4u << 18)));
    COS_SMOKE_CHECK(words[1] == (0x88000u >> 5));

    // EFB colour peeks read as white; the alpha follows the alpha read mode. d_snap takes the top
    // six bits as an object index, 0x3F meaning "none".
    u32 argb = 0;
    GXPokeAlphaRead(GX_READ_NONE);
    GXPeekARGB(10, 20, &argb);
    COS_SMOKE_CHECK((argb >> 26) == 0x3F);
    GXPokeAlphaRead(GX_READ_00);
    GXPeekARGB(10, 20, &argb);
    COS_SMOKE_CHECK(argb == 0x00FFFFFFu);
    GXPokeAlphaRead(GX_READ_FF);
    GXPeekARGB(10, 20, &argb);
    COS_SMOKE_CHECK(argb == 0xFFFFFFFFu);

    // Counters read 0; the settings calls of the game's renderers are accepted.
    u32 m[4] = {1, 2, 3, 4};
    GXSetGPMetric(GX_PERF0_NONE, GX_PERF1_NONE);
    GXClearGPMetric();
    GXReadXfRasMetric(&m[0], &m[1], &m[2], &m[3]);
    COS_SMOKE_CHECK(m[0] == 0 && m[1] == 0 && m[2] == 0 && m[3] == 0);
    GXSetMisc(GX_MT_XF_FLUSH, 8);
    GXSetMisc(GX_MT_XF_FLUSH, 0);
    GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 0);
    GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 1);
    GXSetCopyClamp(static_cast<GXFBClamp>(GX_CLAMP_TOP | GX_CLAMP_BOTTOM));
    GXAbortFrame();
    return true;
}
