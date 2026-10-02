// Shim (phase 2, step 2.4): the bare <types.h> Aurora's <dolphin/gba.h> includes.
//
// Dusklight puts Aurora's include/dolphin on the search path, where <types.h> is
// <dolphin/types.h>. The game's aurora mode does not (it would expose Aurora's bare os.h, gx.h... next
// to the game's own names), so this one name forwards to the same header.
#ifndef COS_SDK_TYPES_H
#define COS_SDK_TYPES_H

#include <dolphin/types.h>

#endif
