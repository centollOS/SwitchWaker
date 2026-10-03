// cos_sdk: GX functions of the GameCube SDK that Aurora at 3227d76 does not declare
// (docs/NATIVE_PORT_PHASE2_3.md, step 2.6c). cos_sdk defines them in src/gx/GXExtras.cpp.
//
// The game sees the same names through the decomp's headers (COS_SDK_HEADERS=decomp) or the
// forwarders of native/include/sdk (aurora mode: GXSetDrawSync in dolphin/gx/GXMisc.h); this header
// is for cos_sdk itself and its tests, which are compiled against Aurora's headers only.
#ifndef COS_SDK_GX_H
#define COS_SDK_GX_H

#include <dolphin/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sends `token` to the pixel engine; when the GPU reaches it, the GXSetDrawSyncCallback callback
// runs with it. On the host the token counts as reached at once (see GXExtras.cpp).
void GXSetDrawSync(u16 token);

// The performance counters of the game's SDK (GXPerf.c: these two plus GXReadXfRasMetric, which
// Aurora declares). On the host there are no counters; every read gives 0.
void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1);
void GXClearGPMetric(void);

#ifdef __cplusplus
}
#endif

#endif // COS_SDK_GX_H
