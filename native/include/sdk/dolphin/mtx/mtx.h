// Forwarder (phase 2, step 2.3): the game's dolphin/mtx/mtx.h over Aurora's <dolphin/mtx.h>.
// The MTX functions and the PSMTX/MTX name mapping are Aurora's (MTX_USE_PS=1, as in Dusklight).
// The decomp's matrix type names Aurora lacks are added here, with the decomp's definitions.
// __PSMTXRotAxisRadInternal (an SDK-internal helper of the paired-single MTX source) is left out.
#ifndef COS_SDK_DOLPHIN_MTX_MTX_H
#define COS_SDK_DOLPHIN_MTX_MTX_H

#include <dolphin/mtx.h>
#include <dolphin/mtx/mtx44.h>
#include <dolphin/mtx/quat.h>
#include "cos_sdk_extras.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef f32 Mtx33[3][3];
typedef f32 Mtx23[2][3];
typedef f32 (*MtxP)[4];
typedef f32 (*Mtx3P)[3];
typedef const f32 (*CMtxP)[4];

#ifdef __cplusplus
}
#endif

#endif
