// What the game's dolphin/types.h gives the game beyond Aurora's <dolphin/types.h>.
//
// Phase 2, step 2.3 (docs/NATIVE_PORT_PHASE2_3.md, decision D2). With COS_SDK_HEADERS=aurora,
// Aurora's <dolphin/types.h> wins over the decomp's, so its game-only names would be lost. This
// header is force-included after native/include/pc/cos_pc_config.h in every game unit (as
// dolphin/types.h reached every unit through global.h), and the forwarders include it too.
//
// Rule: only names Aurora does not declare. A typedef or macro Aurora also has would be a second,
// possibly different definition.
#ifndef COS_SDK_EXTRAS_H
#define COS_SDK_EXTRAS_H

#include <dolphin/types.h>

// Same type as the decomp's typedef (and the host's <sys/types.h> one); a repeated typedef of the
// same type is valid C11 and C++.
typedef unsigned int uint;

// Same text as the decomp's dolphin/types.h, trailing semicolon included.
#define READU32_BE(ptr, offset)                                                                    \
    (((u32)ptr[offset] << 24) | ((u32)ptr[offset + 1] << 16) | ((u32)ptr[offset + 2] << 8) |       \
     (u32)ptr[offset + 3]);

// Set only in aurora mode (this header is force-included only there). Lets a game header pick
// an Aurora spelling (e.g. GXRenderModeObj::aa) while COS_SDK_HEADERS=decomp still exists.
// TODO(native phase 2.8): remove together with decomp mode and the `defined(COS_SDK_AURORA)` tests.
#define COS_SDK_AURORA 1

#define FLOAT_MIN (1.175494351e-38f)
#define FLOAT_MAX (3.40282346638528860e+38f)

#endif // COS_SDK_EXTRAS_H
