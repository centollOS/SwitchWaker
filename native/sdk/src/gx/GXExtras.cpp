// cos_sdk: the GX functions the game needs that Aurora at 3227d76 does not define
// (docs/NATIVE_PORT_PHASE2_3.md, step 2.6c): draw sync tokens, performance counters, GXSetMisc,
// the copy clamp, the XFB line arithmetic, GXAbortFrame, the current GX thread, texture cache
// regions, EFB alpha-read mode and colour peeks, and the FIFO base/size getters. The list is the
// plan's plus the census's SDK/GX bucket (link_census.txt with COS_WITH_AURORA=ON).
//
// Provenance: adapted from the "GX" section of Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight),
// which logs and returns 0/NULL for all of these. Changed:
// - GXGetNumXfbLines and GXGetYScaleFactor compute what the SDK computes (the algorithm of the
//   decomp's src/dolphin/gx/GXFrameBuf.c) instead of returning 0: JUTXfb sizes its frame buffers
//   with them;
// - GXGetCurrentGXThread/GXSetCurrentGXThread keep the GX thread as the SDK does (GXFifo.c) instead
//   of returning NULL: mDoRst_reset cancels the returned thread when it is not the caller;
// - GXSetDrawSyncCallback keeps and returns the previous callback, and GXSetDrawSync delivers the
//   token (Dusklight returns the new callback and never calls it, so a screen capture would only
//   end through its time-out alarm);
// - GXInitTexCacheRegion fills the region as the SDK does (GXTexture.c);
// - GXSetMisc and GXSetCopyClamp keep their values; GXPokeAlphaRead keeps the mode GXPeekARGB applies;
// - GXGetFifoBase/GXGetFifoSize abort instead of returning NULL/0 (see below);
// - added: GXPokeAlphaRead and GXPeekARGB (d_snap); left out: Dusklight's GXRead*Metric/GXClear*Metric
//   beyond the three of the game's SDK (GXPerf.c), GXResetWriteGatherPipe, and GXWaitDrawDone, which
//   in the game's SDK is a static helper of GXDrawDone (GXMisc.c) that no game unit can reference.
//
// Differences from the console a caller can notice, each logged once when first reached:
// - GXSetDrawSync: the token counts as reached when GXSetDrawSync returns (the callback runs
//   inside it, with interrupts disabled as in the token interrupt), not when the GPU gets there.
//   Aurora's public API has no token or fence that the GPU signals, only the draw-done callback
//   that JUTVideo owns. The game's only user is the screen capture of m_Do_graphic.cpp (phase 6 checks
//   that the copied texture is ready by then);
// - GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 0): the SDK then stops saving and restoring the GX state
//   around GXBeginDisplayList/GXEndDisplayList (J3DSys sets it); Aurora keeps its own
//   `dlSaveContext` in a private structure that its public API does not reach, so Aurora goes on
//   restoring it;
// - GXPeekARGB: Aurora can read back depth (GXPeekZ) but not colour. The pixel reads as white with
//   the alpha GXPokeAlphaRead selects (0xFF for GX_READ_NONE), which d_snap reads as "no object".
//   TODO(native phase 6): EFB colour read-back for the picture box;
// - the performance counters read 0. GXAbortFrame has no GPU frame to abort; it logs every call
//   (it is only reached after a GP hang or on reset).
#include "../os/os_internal.h"

#include <dolphin/gx.h>

#include "cos_sdk/gx.h"

using namespace cos_sdk::os;

namespace {

GXDrawSyncCallback sDrawSyncCallback = nullptr; // guarded by Lock()
u16 sDrawSyncToken = 0;                         // guarded by Lock()
OSThread* sGXThread = nullptr;                  // guarded by Lock(); null until first asked
GXAlphaReadMode sAlphaReadMode = GX_READ_NONE;  // guarded by Lock()
u32 sMiscXfFlush = 0;                           // guarded by Lock()
bool sMiscDlSaveContext = true;                 // guarded by Lock()
GXFBClamp sCopyClamp = static_cast<GXFBClamp>(GX_CLAMP_TOP | GX_CLAMP_BOTTOM); // guarded by Lock()

// The GX thread before anyone set it: the thread GXInit ran on. Aurora's GXInit does not record
// it, so it is the default (main) OS thread, the one that runs main and, in the game, GXInit (through
// JFWSystem::init). Needs Lock().
OSThread* GXThreadLocked() {
    if (sGXThread == nullptr) {
        sGXThread = DefaultThreadLocked();
    }
    return sGXThread;
}

// GX_SET_REG of the SDK: bits [first, last] in PowerPC numbering (bit 0 is the MSB).
u32 SetRegField(u32 reg, u32 value, int first, int last) {
    const int width = last - first + 1;
    const int shift = 31 - last;
    const u32 mask = ((width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u)) << shift;
    return (reg & ~mask) | ((value << shift) & mask);
}

// __GXGetNumXfbLines of the SDK (GXFrameBuf.c).
u32 NumXfbLines(u32 height, u32 scale) {
    const u32 numLines = (height - 1) * 0x100;
    u32 actualHeight = (numLines / scale) + 1;
    u32 newScale = scale;
    if (newScale > 0x80 && newScale < 0x100) {
        while (newScale % 2 == 0) {
            newScale /= 2;
        }
        if (height % newScale == 0) {
            actualHeight++;
        }
    }
    if (actualHeight > 0x400) {
        actualHeight = 0x400;
    }
    return actualHeight;
}

u32 YScaleToReg(f32 yScale) {
    return static_cast<u32>(256.0f / yScale) & 0x1FF;
}

// The SDK's internal view of GXTexRegion (__GXTexRegionInt), stored in Aurora's opaque one.
struct TexRegionInt {
    u32 image1;     // even TMEM: base and cache size
    u32 image2;     // odd TMEM: base and cache size
    u32 unused;
    u8 is32bMipmap;
    u8 isCached;
};
static_assert(sizeof(TexRegionInt) <= sizeof(GXTexRegion), "GXTexRegion is too small");

u32 TexCacheSizeReg(GXTexCacheSize size, bool allowNone, const char* which) {
    switch (size) {
    case GX_TEXCACHE_32K:
        return 3;
    case GX_TEXCACHE_128K:
        return 4;
    case GX_TEXCACHE_512K:
        return 5;
    case GX_TEXCACHE_NONE:
        if (allowNone) {
            return 0;
        }
        break;
    }
    Fatal("GXInitTexCacheRegion: invalid %s cache size %d", which, static_cast<int>(size));
}

} // namespace

extern "C" {

// ---- Draw sync ---------------------------------------------------------------------------------

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    Guard guard;
    const GXDrawSyncCallback previous = sDrawSyncCallback;
    sDrawSyncCallback = cb;
    return previous;
}

void GXSetDrawSync(u16 token) {
    GXFlush();
    COS_SDK_LOG_ONCE("GXSetDrawSync: the token counts as reached at once (Aurora signals no "
                     "token from the GPU); the draw sync callback runs inside GXSetDrawSync");
    // The token interrupt: the register takes the token, then the callback runs with interrupts
    // disabled. The callback may replace itself (m_Do_graphic's capture callback does).
    Guard guard;
    sDrawSyncToken = token;
    if (sDrawSyncCallback != nullptr) {
        sDrawSyncCallback(sDrawSyncToken);
    }
}

// ---- Performance counters ----------------------------------------------------------------------

void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1) {
    if (perf0 != GX_PERF0_NONE || perf1 != GX_PERF1_NONE) {
        COS_SDK_LOG_ONCE("GXSetGPMetric(%d, %d): the host has no GPU performance counters; they "
                         "read 0", static_cast<int>(perf0), static_cast<int>(perf1));
    }
}

void GXClearGPMetric(void) {}

void GXReadXfRasMetric(u32* xf_wait_in, u32* xf_wait_out, u32* ras_busy, u32* clocks) {
    COS_SDK_LOG_ONCE("GXReadXfRasMetric: the host has no XF/rasteriser counters; they read 0");
    *xf_wait_in = 0;
    *xf_wait_out = 0;
    *ras_busy = 0;
    *clocks = 0;
}

// ---- Misc settings ----------------------------------------------------------------------------

void GXSetMisc(GXMiscToken token, u32 val) {
    Guard guard;
    switch (token) {
    case GX_MT_NULL:
        break;
    case GX_MT_XF_FLUSH:
        // How many vertices the SDK's flush primitive sends: a GPU workaround with no host
        // counterpart (Aurora's __GXSendFlushPrim writes nothing).
        sMiscXfFlush = val;
        break;
    case GX_MT_DL_SAVE_CONTEXT:
        sMiscDlSaveContext = val != 0;
        if (!sMiscDlSaveContext) {
            COS_SDK_LOG_ONCE("GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 0): recorded, but Aurora keeps "
                             "saving and restoring the GX state around display list recording");
        }
        break;
    default:
        Log("GXSetMisc: unknown token %d (value %u) ignored", static_cast<int>(token),
            static_cast<unsigned>(val));
        break;
    }
}

void GXSetCopyClamp(GXFBClamp clamp) {
    // Which EFB edges the copy's vertical filter clamps at. Aurora applies no copy filter
    // (GXSetCopyFilter is empty), so there is nothing for the clamp to change.
    Guard guard;
    sCopyClamp = clamp;
}

// ---- XFB line arithmetic (the SDK's GXFrameBuf.c) -----------------------------------------------

u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
    return static_cast<u16>(NumXfbLines(efbHeight, YScaleToReg(yScale)));
}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 height1 = xfbHeight;
    f32 scale1 = static_cast<f32>(xfbHeight) / static_cast<f32>(efbHeight);
    u32 height2 = NumXfbLines(efbHeight, YScaleToReg(scale1));

    while (height2 > xfbHeight) {
        height1--;
        scale1 = static_cast<f32>(height1) / static_cast<f32>(efbHeight);
        height2 = NumXfbLines(efbHeight, YScaleToReg(scale1));
    }

    f32 scale2 = scale1;
    while (height2 < xfbHeight) {
        scale2 = scale1;
        height1++;
        scale1 = static_cast<f32>(height1) / static_cast<f32>(efbHeight);
        height2 = NumXfbLines(efbHeight, YScaleToReg(scale1));
    }
    return scale2;
}

// ---- Frame abort and the GX thread --------------------------------------------------------------

void GXAbortFrame(void) {
    // On the console: reset the command processor and drop what is left of the GP FIFO (after a
    // GP hang, or before a reset). Aurora's GPU work has no hang to recover from.
    Log("GXAbortFrame: nothing to abort on the host (Aurora's GPU work is not dropped)");
}

OSThread* GXSetCurrentGXThread(void) {
    Guard guard;
    OSThread* const previous = GXThreadLocked();
    sGXThread = OSGetCurrentThread();
    return previous;
}

OSThread* GXGetCurrentGXThread(void) {
    Guard guard;
    return GXThreadLocked();
}

void* GXGetFifoBase(const GXFifoObj* fifo) {
    (void)fifo;
    // Aurora's GXInitFifoBase and GXInit keep no base or size, so there is no truthful answer.
    Fatal("GXGetFifoBase: Aurora's GX FIFO objects record no base address");
}

u32 GXGetFifoSize(const GXFifoObj* fifo) {
    (void)fifo;
    Fatal("GXGetFifoSize: Aurora's GX FIFO objects record no size");
}

// ---- Texture cache regions (the SDK's GXTexture.c) ----------------------------------------------

void GXInitTexCacheRegion(GXTexRegion* region, GXBool is_32b_mipmap, u32 tmem_even,
                          GXTexCacheSize size_even, u32 tmem_odd, GXTexCacheSize size_odd) {
    auto* internal = reinterpret_cast<TexRegionInt*>(region);

    u32 reg = TexCacheSizeReg(size_even, false, "even");
    internal->image1 = 0;
    internal->image1 = SetRegField(internal->image1, tmem_even >> 5, 17, 31);
    internal->image1 = SetRegField(internal->image1, reg, 14, 16);
    internal->image1 = SetRegField(internal->image1, reg, 11, 13);
    internal->image1 = SetRegField(internal->image1, 0, 10, 10);

    reg = TexCacheSizeReg(size_odd, true, "odd");
    internal->image2 = 0;
    internal->image2 = SetRegField(internal->image2, tmem_odd >> 5, 17, 31);
    internal->image2 = SetRegField(internal->image2, reg, 14, 16);
    internal->image2 = SetRegField(internal->image2, reg, 11, 13);

    internal->unused = 0;
    internal->is32bMipmap = is_32b_mipmap;
    internal->isCached = 1;
}

// ---- EFB alpha read mode and colour peeks -------------------------------------------------------

void GXPokeAlphaRead(GXAlphaReadMode mode) {
    Guard guard;
    sAlphaReadMode = mode;
}

void GXPeekARGB(u16 x, u16 y, u32* color) {
    COS_SDK_LOG_ONCE("GXPeekARGB(%u, %u): Aurora cannot read EFB colours back; pixels read as "
                     "white (TODO(native phase 6))", static_cast<unsigned>(x),
                     static_cast<unsigned>(y));
    GXAlphaReadMode mode;
    {
        Guard guard;
        mode = sAlphaReadMode;
    }
    // ARGB: the alpha byte is what the alpha read mode gives (the EFB's own alpha is unknown, so
    // GX_READ_NONE reads it as 0xFF too).
    const u32 alpha = mode == GX_READ_00 ? 0x00u : 0xFFu;
    *color = (alpha << 24) | 0x00FFFFFFu;
}

} // extern "C"
