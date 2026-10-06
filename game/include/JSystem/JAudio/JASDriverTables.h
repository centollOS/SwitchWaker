#ifndef JASDRIVERTABLES_H
#define JASDRIVERTABLES_H

#include "dolphin/types.h"
#include "global.h"

namespace JASystem {
    namespace Driver {
        extern f32 C5BASE_PITCHTABLE[];
    }
    namespace DSPInterface {
#if TARGET_PC
        // Sized, so that JASDSPInterface's setupBuffer can copy them into MEM1 for the DSP.
        ALIGN_DECL(32, extern u16 DSPADPCM_FILTER[32]);
        ALIGN_DECL(32, extern u16 DSPRES_FILTER[640]);
#else
        ALIGN_DECL(32, extern u16 DSPADPCM_FILTER[]);
        ALIGN_DECL(32, extern u16 DSPRES_FILTER[]);
#endif
    }
}

#endif /* JASDRIVERTABLES_H */
