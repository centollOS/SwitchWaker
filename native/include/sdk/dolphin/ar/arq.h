// Forwarder (phase 2, step 2.4): the game's dolphin/ar/arq.h over Aurora's <dolphin/arq.h>, which
// declares ARQRequest and the ARQ API (aurora_os implements them).
//
// Left out on purpose: the decomp's ARamType and ArqPriotity enums. Their enumerators
// (ARAM_DIR_MRAM_TO_ARAM, ARQ_PRIORITY_LOW...) are macros in Aurora with the same values, so the
// enums cannot be declared again; no game unit names the enum types. Aurora's ARQRequest calls the
// decomp's `destination` field `dest` and its ARQCallback takes a uintptr_t: Aurora's wins and the
// game adapts under TARGET_PC (step 2.7).
#ifndef COS_SDK_DOLPHIN_AR_ARQ_H
#define COS_SDK_DOLPHIN_AR_ARQ_H

#include <dolphin/arq.h>

#endif
