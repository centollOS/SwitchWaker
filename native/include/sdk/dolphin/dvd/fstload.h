// Forwarder (phase 2, step 2.4): the game's dolphin/dvd/fstload.h. Aurora lacks this SDK internal (it
// reads the FST through nod); the declaration is the decomp's (nothing on the host defines it).
#ifndef COS_SDK_DOLPHIN_DVD_FSTLOAD_H
#define COS_SDK_DOLPHIN_DVD_FSTLOAD_H

#include <dolphin/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

void __fstLoad(void);

#ifdef __cplusplus
}
#endif

#endif
