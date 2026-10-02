// SDK header check (phase 2, step 2.3; docs/NATIVE_PORT_PHASE2_3.md, decision D2).
//
// One unit that includes every SDK header name the decomp has under game/include/dolphin,
// spelled as the game spells them, plus the top-level game headers that include SDK headers.
// It builds in both header modes (target cos_sdk_header_check):
// - COS_SDK_HEADERS=decomp: the names resolve to the decomp's own headers;
// - COS_SDK_HEADERS=aurora: each name resolves to Aurora (names both have) or to a forwarder in
//   native/include/sdk (game-only names), and check/check_sdk_shadow.sh fails if any dependency of
//   this unit resolves under game/include/dolphin. That script also checks that every name
//   under game/include/dolphin is listed here.
//
// Steps 2.3 and 2.4 added the forwarders for all of them, so every name is checked in both modes.
// A name added later without a forwarder can sit between COS_SDK_PENDING_BEGIN/END (decomp mode
// only); the shadow script reports such names as pending.

// What every game unit starts with, and the top-level headers that include SDK headers (a quoted
// include there is looked up next to the header first, i.e. in game/include/dolphin).
#include "global.h"
#include "DynamicLink.h"
#include "weak_bss_3569.h"

// Names forwarded or provided by Aurora.
#include <dolphin/ai/ai.h>
#include <dolphin/amcstubs/AmcExi2Stubs.h>
#include <dolphin/ar/ar.h>
#include <dolphin/ar/arq.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/card.h>
#include <dolphin/db/db.h>
#include <dolphin/dsp.h>
#include <dolphin/dvd/dvd.h>
#include <dolphin/dvd/dvderror.h>
#include <dolphin/dvd/dvdFatal.h>
#include <dolphin/dvd/dvdfs.h>
#include <dolphin/dvd/dvdidutils.h>
#include <dolphin/dvd/dvdlow.h>
#include <dolphin/dvd/dvdqueue.h>
#include <dolphin/dvd/fstload.h>
#include <dolphin/exi/EXIBios.h>
#include <dolphin/exi/EXIUart.h>
#include <dolphin/gba/GBA.h>
#include <dolphin/gba/GBAPriv.h>
#include <dolphin/gd/GDBase.h>
#include <dolphin/gd/GDGeometry.h>
#include <dolphin/gd/GDLight.h>
#include <dolphin/gf/GF.h>
#include <dolphin/gf/GFGeometry.h>
#include <dolphin/gf/GFLight.h>
#include <dolphin/gf/GFPixel.h>
#include <dolphin/gf/GFTev.h>
#include <dolphin/gf/GFTransform.h>
#include <dolphin/gx/GX.h>
#include <dolphin/gx/GXAttr.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXDisplayList.h>
#include <dolphin/gx/GXDraw.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXFrameBuf.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXInit.h>
#include <dolphin/gx/GXLight.h>
#include <dolphin/gx/GXMisc.h>
#include <dolphin/gx/GXPerf.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/mtx/mtx.h>
#include <dolphin/mtx/mtx44.h>
#include <dolphin/mtx/mtxvec.h>
#include <dolphin/mtx/quat.h>
#include <dolphin/mtx/vec.h>
#include <dolphin/os/OS.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/os/OSAlloc.h>
#include <dolphin/os/OSArena.h>
#include <dolphin/os/OSAudioSystem.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os/OSContext.h>
#include <dolphin/os/OSError.h>
#include <dolphin/os/OSFont.h>
#include <dolphin/os/OSInterrupt.h>
#include <dolphin/os/OSLink.h>
#include <dolphin/os/OSMemory.h>
#include <dolphin/os/OSMessage.h>
#include <dolphin/os/OSMutex.h>
#include <dolphin/os/OSReboot.h>
#include <dolphin/os/OSReset.h>
#include <dolphin/os/OSResetSW.h>
#include <dolphin/os/OSRtc.h>
#include <dolphin/os/OSStopwatch.h>
#include <dolphin/os/OSSync.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSTime.h>
#include <dolphin/os/OSUtil.h>
#include <dolphin/pad/Pad.h>
#include <dolphin/si/SIBios.h>
#include <dolphin/thp.h>
#include <dolphin/types.h>
#include <dolphin/vi/vi.h>

// Names the decomp's own headers cannot compile here, so checked in aurora mode only:
// __start.h uses Metrowerks' __declspec(weak), and Padclamp.h defines PADClampRegion again after
// Pad.h (no game unit includes either; the forwarders compile in both orders).
#if defined(COS_SDK_HEADERS_AURORA)
#include <dolphin/os/__start.h>
#include <dolphin/pad/Padclamp.h>
#endif

#if defined(COS_SDK_HEADERS_AURORA)
// The game-only names the force-included cos_sdk_extras.h restores.
static_assert(sizeof(uint) == 4, "uint");
static_assert(FLOAT_MAX > 3.4e38f && FLOAT_MIN < 1.2e-38f, "FLOAT_MIN/FLOAT_MAX");
#endif

int cos_sdk_header_check(const u8* p) {
    u32 v = READU32_BE(p, 0)
    Mtx m;
    MTXIdentity(m);
    MtxP mp = m;
    return (int)(v + (u32)mp[0][0]) + (OSGetConsoleType() != 0);
}

// game-only names the forwarders of step 2.4 restore in aurora mode (the decomp's headers declare
// them too, so this compiles in both modes). The unit is an object library and is never linked.
int cos_sdk_header_check_cos_names(DVDFileInfo* fi, Mtx m) {
    DVDDirectory dir;
    DVDDirectoryEntry entry;
    SIPacket packet;
    GXCopyMode copyMode = GX_COPY_PROGRESSIVE;
    GXColor4x8(0xFF, 0xFF, 0xFF, 0xFF);
    GXSetDrawSync(0);
    GFLoadPosMtxImm(m, 0);
    GFWriteCPCmd(0x30, 0);
    return (int)DVDGetLength(fi) + (int)sizeof(dir) + (int)sizeof(entry) + (int)sizeof(packet) +
           (int)copyMode + (EXI_STATE_LOCKED != 0) + GBA_READY + (SIGetType(0) != 0);
}
